// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_manager.h"

#include <algorithm>
#include <utility>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/background_sync/background_sync_metrics.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/android/background_sync_network_observer_android.h"
#include "content/browser/background_sync/background_sync_launcher.h"
#endif

using blink::mojom::BackgroundSyncType;
using blink::mojom::PermissionStatus;
using SyncAndNotificationPermissions =
    std::pair<PermissionStatus, PermissionStatus>;

namespace content {

// TODO(crbug.com/40614176): Use blink::mojom::BackgroundSyncError
// directly and eliminate these checks.
#define COMPILE_ASSERT_MATCHING_ENUM(mojo_name, manager_name) \
  static_assert(static_cast<int>(blink::mojo_name) ==         \
                    static_cast<int>(content::manager_name),  \
                "mojo and manager enums must match")

COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::NONE,
                             BACKGROUND_SYNC_STATUS_OK);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::STORAGE,
                             BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::NOT_FOUND,
                             BACKGROUND_SYNC_STATUS_NOT_FOUND);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::NO_SERVICE_WORKER,
                             BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::NOT_ALLOWED,
                             BACKGROUND_SYNC_STATUS_NOT_ALLOWED);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::PERMISSION_DENIED,
                             BACKGROUND_SYNC_STATUS_PERMISSION_DENIED);
COMPILE_ASSERT_MATCHING_ENUM(mojom::BackgroundSyncError::MAX,
                             BACKGROUND_SYNC_STATUS_PERMISSION_DENIED);

namespace {

// The only allowed value of min_interval for one shot Background Sync
// registrations.
constexpr int kMinIntervalForOneShotSync = -1;

// The key used to index the background sync data in ServiceWorkerStorage.
const char kBackgroundSyncUserDataKey[] = "BackgroundSyncUserData";

void RecordFailureAndPostError(
    BackgroundSyncType sync_type,
    BackgroundSyncStatus status,
    BackgroundSyncManager::StatusAndRegistrationCallback callback) {
  BackgroundSyncMetrics::CountRegisterFailure(sync_type, status);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status, nullptr));
}

// Returns nullptr if the browser context cannot be accessed for any reason.
BrowserContext* GetBrowserContext(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!service_worker_context)
    return nullptr;
  StoragePartitionImpl* storage_partition_impl =
      service_worker_context->storage_partition();
  if (!storage_partition_impl)  // may be null in tests
    return nullptr;

  return storage_partition_impl->browser_context();
}

// Returns nullptr if the controller cannot be accessed for any reason.
BackgroundSyncController* GetBackgroundSyncController(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      GetBrowserContext(std::move(service_worker_context));
  if (!browser_context)
    return nullptr;

  return browser_context->GetBackgroundSyncController();
}

SyncAndNotificationPermissions GetBackgroundSyncPermission(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    const url::Origin& origin,
    RenderProcessHost* render_process_host,
    BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      GetBrowserContext(std::move(service_worker_context));
  if (!browser_context)
    return {PermissionStatus::DENIED, PermissionStatus::DENIED};

  PermissionController* permission_controller =
      browser_context->GetPermissionController();
  DCHECK(permission_controller);

  // The requesting origin always matches the embedding origin.
  auto sync_permission = permission_controller->GetPermissionStatusForWorker(
      sync_type == BackgroundSyncType::ONE_SHOT
          ? blink::PermissionType::BACKGROUND_SYNC
          : blink::PermissionType::PERIODIC_BACKGROUND_SYNC,
      render_process_host, origin);
  auto notification_permission =
      permission_controller->GetPermissionStatusForWorker(
          blink::PermissionType::NOTIFICATIONS, render_process_host, origin);
  return {sync_permission, notification_permission};
}

void NotifyOneShotBackgroundSyncRegistered(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const url::Origin& origin,
    bool can_fire,
    bool is_reregistered) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncController(std::move(sw_context_wrapper));

  if (!background_sync_controller)
    return;

  background_sync_controller->NotifyOneShotBackgroundSyncRegistered(
      origin, can_fire, is_reregistered);
}

void NotifyPeriodicBackgroundSyncRegistered(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const url::Origin& origin,
    int min_interval,
    bool is_reregistered) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncController(std::move(sw_context_wrapper));

  if (!background_sync_controller)
    return;

  background_sync_controller->NotifyPeriodicBackgroundSyncRegistered(
      origin, min_interval, is_reregistered);
}

void NotifyOneShotBackgroundSyncCompleted(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncController(std::move(sw_context_wrapper));

  if (!background_sync_controller)
    return;

  background_sync_controller->NotifyOneShotBackgroundSyncCompleted(
      origin, status_code, num_attempts, max_attempts);
}

void NotifyPeriodicBackgroundSyncCompleted(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncController(std::move(sw_context_wrapper));

  if (!background_sync_controller)
    return;

  background_sync_controller->NotifyPeriodicBackgroundSyncCompleted(
      origin, status_code, num_attempts, max_attempts);
}

std::unique_ptr<BackgroundSyncParameters> GetControllerParameters(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    std::unique_ptr<BackgroundSyncParameters> parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncController(sw_context_wrapper);

  if (!background_sync_controller) {
    // If there is no controller then BackgroundSync can't run in the
    // background, disable it.
    parameters->disable = true;
    return parameters;
  }

  background_sync_controller->GetParameterOverrides(parameters.get());
  return parameters;
}

base::TimeDelta GetNextEventDelay(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const BackgroundSyncRegistration& registration,
    std::unique_ptr<BackgroundSyncParameters> parameters,
    base::TimeDelta time_till_soonest_scheduled_event_for_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncController(sw_context_wrapper);

  if (!background_sync_controller)
    return base::TimeDelta::Max();

  return background_sync_controller->GetNextEventDelay(
      registration, parameters.get(),
      time_till_soonest_scheduled_event_for_origin);
}

void OnSyncEventFinished(scoped_refptr<ServiceWorkerVersion> active_version,
                         int request_id,
                         ServiceWorkerVersion::StatusCallback callback,
                         blink::mojom::ServiceWorkerEventStatus status) {
  if (!active_version->FinishRequest(
          request_id,
          status == blink::mojom::ServiceWorkerEventStatus::COMPLETED)) {
    return;
  }
  std::move(callback).Run(
      mojo::ConvertTo<blink::ServiceWorkerStatusCode>(status));
}

void DidStartWorkerForSyncEvent(
    base::OnceCallback<void(ServiceWorkerVersion::StatusCallback)> task,
    ServiceWorkerVersion::StatusCallback callback,
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(start_worker_status);
    return;
  }
  std::move(task).Run(std::move(callback));
}

BackgroundSyncType GetBackgroundSyncType(
    const blink::mojom::SyncRegistrationOptions& options) {
  return options.min_interval == -1 ? BackgroundSyncType::ONE_SHOT
                                    : BackgroundSyncType::PERIODIC;
}

std::string GetSyncEventName(const BackgroundSyncType sync_type) {
  if (sync_type == BackgroundSyncType::ONE_SHOT)
    return "sync";
  else
    return "periodicsync";
}

DevToolsBackgroundService GetDevToolsBackgroundService(
    BackgroundSyncType sync_type) {
  if (sync_type == BackgroundSyncType::ONE_SHOT)
    return DevToolsBackgroundService::kBackgroundSync;
  else
    return DevToolsBackgroundService::kPeriodicBackgroundSync;
}

std::string GetDelayAsString(base::TimeDelta delay) {
  if (delay.is_max())
    return "infinite";
  return base::NumberToString(delay.InMilliseconds());
}

std::string GetEventStatusString(blink::ServiceWorkerStatusCode status_code) {
  // The |status_code| is derived from blink::mojom::ServiceWorkerEventStatus.
  switch (status_code) {
    case blink::ServiceWorkerStatusCode::kOk:
      return "succeeded";
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
      return "waitUntil rejected";
    case blink::ServiceWorkerStatusCode::kErrorFailed:
      return "failed";
    case blink::ServiceWorkerStatusCode::kErrorAbort:
      return "aborted";
    case blink::ServiceWorkerStatusCode::kErrorTimeout:
      return "timeout";
    default:
      SCOPED_CRASH_KEY_NUMBER("BGSM", "status_code",
                              static_cast<int>(status_code));
      DUMP_WILL_BE_NOTREACHED()
          << "status_code " << static_cast<int>(status_code);
      return "unknown error";
  }
}

int GetNumAttemptsAfterEvent(BackgroundSyncType sync_type,
                             int current_num_attempts,
                             int max_attempts,
                             blink::mojom::BackgroundSyncState sync_state,
                             bool succeeded) {
  int num_attempts = ++current_num_attempts;

  if (sync_type == BackgroundSyncType::PERIODIC) {
    if (succeeded)
      return 0;
    if (num_attempts == max_attempts)
      return 0;
  }

  if (sync_state ==
      blink::mojom::BackgroundSyncState::REREGISTERED_WHILE_FIRING) {
    return 0;
  }

  return num_attempts;
}

// This prevents the browser process from shutting down when the last browser
// window is closed and there are one-shot Background Sync events ready to fire.
std::unique_ptr<BackgroundSyncController::BackgroundSyncEventKeepAlive>
CreateBackgroundSyncEventKeepAlive(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const blink::mojom::BackgroundSyncRegistrationInfo& registration_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* controller =
      GetBackgroundSyncController(sw_context_wrapper);
  if (!controller ||
      registration_info.sync_type != BackgroundSyncType::ONE_SHOT) {
    return nullptr;
  }

  return controller->CreateBackgroundSyncEventKeepAlive();
}

}  // namespace

BackgroundSyncManager::BackgroundSyncRegistrations::
    BackgroundSyncRegistrations() = default;

BackgroundSyncManager::BackgroundSyncRegistrations::BackgroundSyncRegistrations(
    const BackgroundSyncRegistrations& other) = default;

BackgroundSyncManager::BackgroundSyncRegistrations::
    ~BackgroundSyncRegistrations() = default;

// static
std::unique_ptr<BackgroundSyncManager> BackgroundSyncManager::Create(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    DevToolsBackgroundServicesContextImpl& devtools_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncManager* sync_manager = new BackgroundSyncManager(
      std::move(service_worker_context), devtools_context);
  sync_manager->Init();
  return base::WrapUnique(sync_manager);
}

BackgroundSyncManager::~BackgroundSyncManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  service_worker_context_->RemoveObserver(this);
}

void BackgroundSyncManager::Register(
    int64_t sw_registration_id,
    int render_process_host_id,
    blink::mojom::SyncRegistrationOptions options,
    StatusAndRegistrationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_) {
    RecordFailureAndPostError(GetBackgroundSyncType(options),
                              BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              std::move(callback));
    return;
  }

  DCHECK(options.min_interval >= 0 ||
         options.min_interval == kMinIntervalForOneShotSync);

  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::RegisterCheckIfHasMainFrame,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     render_process_host_id, std::move(options),
                     op_scheduler_.WrapCallbackToRunNext(std::move(callback))));
}

void BackgroundSyncManager::UnregisterPeriodicSync(
    int64_t sw_registration_id,
    const std::string& tag,
    BackgroundSyncManager::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  BACKGROUND_SYNC_STATUS_STORAGE_ERROR));
    return;
  }

  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::UnregisterPeriodicSyncImpl,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id, tag,
                     op_scheduler_.WrapCallbackToRunNext(std::move(callback))));
}

void BackgroundSyncManager::DidResolveRegistration(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_)
    return;
  op_scheduler_.ScheduleOperation(base::BindOnce(
      &BackgroundSyncManager::DidResolveRegistrationImpl,
      weak_ptr_factory_.GetWeakPtr(), std::move(registration_info)));
}

void BackgroundSyncManager::GetOneShotSyncRegistrations(
    int64_t sw_registration_id,
    StatusAndRegistrationsCallback callback) {
  GetRegistrations(BackgroundSyncType::ONE_SHOT, sw_registration_id,
                   std::move(callback));
}

void BackgroundSyncManager::GetPeriodicSyncRegistrations(
    int64_t sw_registration_id,
    StatusAndRegistrationsCallback callback) {
  GetRegistrations(BackgroundSyncType::PERIODIC, sw_registration_id,
                   std::move(callback));
}

void BackgroundSyncManager::UnregisterPeriodicSyncForOrigin(
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::UnregisterForOriginImpl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(origin),
                     MakeEmptyCompletion()));
}

void BackgroundSyncManager::UnregisterForOriginImpl(
    const url::Origin& origin,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  std::vector<int64_t> service_worker_registrations_affected;

  for (const auto& service_worker_and_registration : active_registrations_) {
    const auto registrations = service_worker_and_registration.second;
    if (registrations.origin != origin)
      continue;

    service_worker_registrations_affected.emplace_back(
        service_worker_and_registration.first);
  }

  if (service_worker_registrations_affected.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      service_worker_registrations_affected.size(),
      base::BindOnce(
          &BackgroundSyncManager::UnregisterForOriginScheduleDelayedProcessing,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  for (int64_t service_worker_registration_id :
       service_worker_registrations_affected) {
    StoreRegistrations(
        service_worker_registration_id,
        base::BindOnce(&BackgroundSyncManager::UnregisterForOriginDidStore,
                       weak_ptr_factory_.GetWeakPtr(),
                       service_worker_registration_id, barrier_closure));
  }
}

void BackgroundSyncManager::UnregisterForOriginDidStore(
    int64_t service_worker_registration_id_to_remove,
    base::OnceClosure done_closure,
    blink::ServiceWorkerStatusCode status) {
  active_registrations_.erase(service_worker_registration_id_to_remove);

  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // The service worker registration is gone.
    std::move(done_closure).Run();
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    DisableAndClearManager(std::move(done_closure));
    return;
  }

  std::move(done_closure).Run();
}

void BackgroundSyncManager::UnregisterForOriginScheduleDelayedProcessing(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleOrCancelDelayedProcessing(BackgroundSyncType::PERIODIC);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void BackgroundSyncManager::GetRegistrations(
    BackgroundSyncType sync_type,
    int64_t sw_registration_id,
    StatusAndRegistrationsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The renderer should have checked and disallowed the request for fenced
  // frames and thrown an exception in blink::SyncManager or
  // blink::PeriodicSyncManager. Return a not allowed error if the renderer side
  // check didn't happen for some reason.
  scoped_refptr<ServiceWorkerRegistration> sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (sw_registration && sw_registration->ancestor_frame_type() ==
                             blink::mojom::AncestorFrameType::kFencedFrame) {
    mojo::ReportBadMessage("Background Sync is not allowed in a fenced frame");
    return;
  }

  if (disabled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
            std::vector<std::unique_ptr<BackgroundSyncRegistration>>()));
    return;
  }

  op_scheduler_.ScheduleOperation(base::BindOnce(
      &BackgroundSyncManager::GetRegistrationsImpl,
      weak_ptr_factory_.GetWeakPtr(), sync_type, sw_registration_id,
      op_scheduler_.WrapCallbackToRunNext(std::move(callback))));
}

void BackgroundSyncManager::OnRegistrationDeleted(
    int64_t sw_registration_id,
    const GURL& pattern,
    const blink::StorageKey& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Operations already in the queue will either fail when they write to storage
  // or return stale results based on registrations loaded in memory. This is
  // inconsequential since the service worker is gone.
  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::OnRegistrationDeletedImpl,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     MakeEmptyCompletion()));
}

void BackgroundSyncManager::OnStorageWiped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Operations already in the queue will either fail when they write to storage
  // or return stale results based on registrations loaded in memory. This is
  // inconsequential since the service workers are gone.
  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::OnStorageWipedImpl,
                     weak_ptr_factory_.GetWeakPtr(), MakeEmptyCompletion()));
}

void BackgroundSyncManager::EmulateDispatchSyncEvent(
    const std::string& tag,
    scoped_refptr<ServiceWorkerVersion> active_version,
    bool last_chance,
    ServiceWorkerVersion::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blink::ServiceWorkerStatusCode code = CanEmulateSyncEvent(active_version);
  if (code != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(code);
    return;
  }

  DispatchSyncEvent(tag, std::move(active_version), last_chance,
                    std::move(callback));
}

void BackgroundSyncManager::EmulateDispatchPeriodicSyncEvent(
    const std::string& tag,
    scoped_refptr<ServiceWorkerVersion> active_version,
    ServiceWorkerVersion::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blink::ServiceWorkerStatusCode code = CanEmulateSyncEvent(active_version);
  if (code != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(code);
    return;
  }

  DispatchPeriodicSyncEvent(tag, std::move(active_version),
                            std::move(callback));
}

void BackgroundSyncManager::EmulateServiceWorkerOffline(
    int64_t service_worker_id,
    bool is_offline) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Multiple DevTools sessions may want to set the same SW offline, which
  // is supposed to disable the background sync. For consistency with the
  // network stack, SW remains offline until all DevTools sessions disable
  // the offline mode.
  emulated_offline_sw_[service_worker_id] += is_offline ? 1 : -1;
  if (emulated_offline_sw_[service_worker_id] > 0)
    return;
  emulated_offline_sw_.erase(service_worker_id);
  FireReadyEvents(BackgroundSyncType::ONE_SHOT, /* reschedule= */ true,
                  base::DoNothing());
}

BackgroundSyncManager::BackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    DevToolsBackgroundServicesContextImpl& devtools_context)
    : op_scheduler_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      service_worker_context_(std::move(service_worker_context)),
      proxy_(std::make_unique<BackgroundSyncProxy>(service_worker_context_)),
      devtools_context_(&devtools_context),
      parameters_(std::make_unique<BackgroundSyncParameters>()),
      disabled_(false),
      num_firing_registrations_one_shot_(0),
      num_firing_registrations_periodic_(0),
      clock_(base::DefaultClock::GetInstance()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(devtools_context_);
  DCHECK(service_worker_context_);

  service_worker_context_->AddObserver(this);

#if BUILDFLAG(IS_ANDROID)
  network_observer_ = std::make_unique<BackgroundSyncNetworkObserverAndroid>(
      base::BindRepeating(&BackgroundSyncManager::OnNetworkChanged,
                          weak_ptr_factory_.GetWeakPtr()));
#else
  network_observer_ = std::make_unique<BackgroundSyncNetworkObserver>(
      base::BindRepeating(&BackgroundSyncManager::OnNetworkChanged,
                          weak_ptr_factory_.GetWeakPtr()));
#endif
}

void BackgroundSyncManager::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!op_scheduler_.ScheduledOperations());
  DCHECK(!disabled_);

  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::InitImpl,
                     weak_ptr_factory_.GetWeakPtr(), MakeEmptyCompletion()));
}

void BackgroundSyncManager::InitImpl(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  InitDidGetControllerParameters(
      std::move(callback),
      GetControllerParameters(
          service_worker_context_,
          std::make_unique<BackgroundSyncParameters>(*parameters_)));
}

void BackgroundSyncManager::InitDidGetControllerParameters(
    base::OnceClosure callback,
    std::unique_ptr<BackgroundSyncParameters> updated_parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  parameters_ = std::move(updated_parameters);
  if (parameters_->disable) {
    disabled_ = true;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  network_observer_->Init();

  GetDataFromBackend(
      kBackgroundSyncUserDataKey,
      base::BindOnce(&BackgroundSyncManager::InitDidGetDataFromBackend,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncManager::InitDidGetDataFromBackend(
    base::OnceClosure callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != blink::ServiceWorkerStatusCode::kOk &&
      status != blink::ServiceWorkerStatusCode::kErrorNotFound) {
    DisableAndClearManager(std::move(callback));
    return;
  }

  std::set<url::Origin> suspended_periodic_sync_origins;
  std::set<url::Origin> registered_origins;
  for (const std::pair<int64_t, std::string>& data : user_data) {
    BackgroundSyncRegistrationsProto registrations_proto;
    if (registrations_proto.ParseFromString(data.second)) {
      BackgroundSyncRegistrations* registrations =
          &active_registrations_[data.first];
      registrations->origin =
          url::Origin::Create(GURL(registrations_proto.origin()));

      for (const auto& registration_proto :
           registrations_proto.registration()) {
        BackgroundSyncType sync_type =
            registration_proto.has_periodic_sync_options()
                ? BackgroundSyncType::PERIODIC
                : BackgroundSyncType::ONE_SHOT;
        BackgroundSyncRegistration* registration =
            &registrations
                 ->registration_map[{registration_proto.tag(), sync_type}];

        blink::mojom::SyncRegistrationOptions* options =
            registration->options();
        options->tag = registration_proto.tag();
        if (sync_type == BackgroundSyncType::PERIODIC) {
          options->min_interval =
              registration_proto.periodic_sync_options().min_interval();
          if (options->min_interval < 0) {
            DisableAndClearManager(std::move(callback));
            return;
          }
        } else {
          options->min_interval = kMinIntervalForOneShotSync;
        }

        registration->set_num_attempts(registration_proto.num_attempts());
        registration->set_delay_until(
            base::Time::FromInternalValue(registration_proto.delay_until()));
        registration->set_origin(registrations->origin);
        registered_origins.insert(registration->origin());
        if (registration->is_suspended()) {
          suspended_periodic_sync_origins.insert(registration->origin());
        }
        registration->set_resolved();
        if (registration_proto.has_max_attempts())
          registration->set_max_attempts(registration_proto.max_attempts());
        else
          registration->set_max_attempts(parameters_->max_sync_attempts);
      }
    }
  }

  FireReadyEvents(BackgroundSyncType::ONE_SHOT, /* reschedule= */ true,
                  base::DoNothing());
  FireReadyEvents(BackgroundSyncType::PERIODIC, /* reschedule= */ true,
                  base::DoNothing());
  proxy_->SendSuspendedPeriodicSyncOrigins(
      std::move(suspended_periodic_sync_origins));
  proxy_->SendRegisteredPeriodicSyncOrigins(std::move(registered_origins));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void BackgroundSyncManager::RegisterCheckIfHasMainFrame(
    int64_t sw_registration_id,
    int render_process_host_id,
    blink::mojom::SyncRegistrationOptions options,
    StatusAndRegistrationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<ServiceWorkerRegistration> sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    RecordFailureAndPostError(GetBackgroundSyncType(options),
                              BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  // The renderer should have checked and disallowed the request for fenced
  // frames and thrown an exception in blink::SyncManager or
  // blink::PeriodicSyncManager. Return a not allowed error if the renderer side
  // check didn't happen for some reason.
  if (sw_registration->ancestor_frame_type() ==
      blink::mojom::AncestorFrameType::kFencedFrame) {
    mojo::ReportBadMessage("Background Sync is not allowed in a fenced frame");
    return;
  }

  HasMainFrameWindowClient(
      sw_registration->key(),
      base::BindOnce(&BackgroundSyncManager::RegisterDidCheckIfMainFrame,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     render_process_host_id, std::move(options),
                     std::move(callback)));
}

void BackgroundSyncManager::RegisterDidCheckIfMainFrame(
    int64_t sw_registration_id,
    int render_process_host_id,
    blink::mojom::SyncRegistrationOptions options,
    StatusAndRegistrationCallback callback,
    bool has_main_frame_client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!has_main_frame_client) {
    RecordFailureAndPostError(GetBackgroundSyncType(options),
                              BACKGROUND_SYNC_STATUS_NOT_ALLOWED,
                              std::move(callback));
    return;
  }
  RegisterImpl(sw_registration_id, render_process_host_id, std::move(options),
               std::move(callback));
}

void BackgroundSyncManager::RegisterImpl(
    int64_t sw_registration_id,
    int render_process_host_id,
    blink::mojom::SyncRegistrationOptions options,
    StatusAndRegistrationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_) {
    RecordFailureAndPostError(GetBackgroundSyncType(options),
                              BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              std::move(callback));
    return;
  }

  if (options.tag.length() > kMaxTagLength) {
    RecordFailureAndPostError(GetBackgroundSyncType(options),
                              BACKGROUND_SYNC_STATUS_NOT_ALLOWED,
                              std::move(callback));
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    RecordFailureAndPostError(GetBackgroundSyncType(options),
                              BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(render_process_host_id);
  if (!render_process_host) {
    RecordFailureAndPostError(GetBackgroundSyncType(options),
                              BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  BackgroundSyncType sync_type = GetBackgroundSyncType(options);

  if (parameters_->skip_permissions_check_for_testing) {
    RegisterDidAskForPermission(
        sw_registration_id, std::move(options), std::move(callback),
        {PermissionStatus::GRANTED, PermissionStatus::GRANTED});
    return;
  }

  SyncAndNotificationPermissions permission = GetBackgroundSyncPermission(
      service_worker_context_, sw_registration->key().origin(),
      render_process_host, sync_type);
  RegisterDidAskForPermission(sw_registration_id, std::move(options),
                              std::move(callback), permission);
}

void BackgroundSyncManager::RegisterDidAskForPermission(
    int64_t sw_registration_id,
    blink::mojom::SyncRegistrationOptions options,
    StatusAndRegistrationCallback callback,
    SyncAndNotificationPermissions permission_statuses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (permission_statuses.first == PermissionStatus::DENIED) {
    RecordFailureAndPostError(GetBackgroundSyncType(options),
                              BACKGROUND_SYNC_STATUS_PERMISSION_DENIED,
                              std::move(callback));
    return;
  }
  DCHECK_EQ(permission_statuses.first, PermissionStatus::GRANTED);

  scoped_refptr<ServiceWorkerRegistration> sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    // The service worker was shut down in the interim.
    RecordFailureAndPostError(GetBackgroundSyncType(options),
                              BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  BackgroundSyncRegistration* existing_registration =
      LookupActiveRegistration(blink::mojom::BackgroundSyncRegistrationInfo(
          sw_registration_id, options.tag, GetBackgroundSyncType(options)));

  const url::Origin& origin = sw_registration->key().origin();

  if (GetBackgroundSyncType(options) ==
      blink::mojom::BackgroundSyncType::ONE_SHOT) {
    bool is_reregistered =
        existing_registration && existing_registration->IsFiring();
    NotifyOneShotBackgroundSyncRegistered(
        service_worker_context_, origin,
        /* can_fire= */ AreOptionConditionsMet(), is_reregistered);
  } else {
    NotifyPeriodicBackgroundSyncRegistered(
        service_worker_context_, origin, options.min_interval,
        /* is_reregistered= */ static_cast<bool>(existing_registration));
  }

  if (existing_registration) {
    DCHECK_EQ(existing_registration->options()->tag, options.tag);
    DCHECK_EQ(existing_registration->sync_type(),
              GetBackgroundSyncType(options));

    if (existing_registration->options()->Equals(options)) {
      BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
          AreOptionConditionsMet()
              ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
              : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;
      BackgroundSyncMetrics::CountRegisterSuccess(
          existing_registration->sync_type(), options.min_interval,
          registration_could_fire,
          BackgroundSyncMetrics::REGISTRATION_IS_DUPLICATE);

      if (existing_registration->IsFiring()) {
        existing_registration->set_sync_state(
            blink::mojom::BackgroundSyncState::REREGISTERED_WHILE_FIRING);
      }

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), BACKGROUND_SYNC_STATUS_OK,
                         std::make_unique<BackgroundSyncRegistration>(
                             *existing_registration)));
      return;
    }
  }

  BackgroundSyncRegistration registration;

  registration.set_origin(origin);
  *registration.options() = std::move(options);

  // TODO(crbug.com/40627578): This section below is really confusing. Add a
  // comment explaining what's going on here, or annotate permission_statuses.
  registration.set_max_attempts(
      permission_statuses.second == PermissionStatus::GRANTED
          ? parameters_->max_sync_attempts_with_notification_permission
          : parameters_->max_sync_attempts);

  // Skip the current registration when getting time till next scheduled
  // periodic sync event for the origin. This is because we'll be updating the
  // schedule time of this registration soon anyway, so considering its
  // schedule time would cause us to calculate incorrect delay.
  if (registration.sync_type() == BackgroundSyncType::PERIODIC) {
    base::TimeDelta delay = GetNextEventDelay(
        service_worker_context_, registration,
        std::make_unique<BackgroundSyncParameters>(*parameters_),
        GetSmallestPeriodicSyncEventDelayForOrigin(
            origin, registration.options()->tag));
    RegisterDidGetDelay(sw_registration_id, registration, std::move(callback),
                        delay);
    return;
  }

  RegisterDidGetDelay(sw_registration_id, registration, std::move(callback),
                      base::TimeDelta());
}

void BackgroundSyncManager::RegisterDidGetDelay(
    int64_t sw_registration_id,
    BackgroundSyncRegistration registration,
    StatusAndRegistrationCallback callback,
    base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We don't fire periodic Background Sync registrations immediately after
  // registration, so set delay_until to override its default value.
  if (registration.sync_type() == BackgroundSyncType::PERIODIC)
    registration.set_delay_until(clock_->Now() + delay);

  scoped_refptr<ServiceWorkerRegistration> sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    // The service worker was shut down in the interim.
    RecordFailureAndPostError(registration.sync_type(),
                              BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  if (registration.sync_type() == BackgroundSyncType::PERIODIC &&
      ShouldLogToDevTools(registration.sync_type())) {
    devtools_context_->LogBackgroundServiceEvent(
        sw_registration_id,
        blink::StorageKey::CreateFirstParty(registration.origin()),
        DevToolsBackgroundService::kPeriodicBackgroundSync,
        /* event_name= */ "Got next event delay",
        /* instance_id= */ registration.options()->tag,
        {{"Next Attempt Delay (ms)",
          GetDelayAsString(registration.delay_until() - clock_->Now())}});
  }

  AddOrUpdateActiveRegistration(sw_registration_id,
                                sw_registration->key().origin(), registration);

  StoreRegistrations(
      sw_registration_id,
      base::BindOnce(&BackgroundSyncManager::RegisterDidStore,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     registration, std::move(callback)));
}

void BackgroundSyncManager::UnregisterPeriodicSyncImpl(
    int64_t sw_registration_id,
    const std::string& tag,
    BackgroundSyncManager::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto registration_info = blink::mojom::BackgroundSyncRegistrationInfo(
      sw_registration_id, tag, BackgroundSyncType::PERIODIC);

  if (!LookupActiveRegistration(registration_info)) {
    // It's okay to not find a matching tag.
    UnregisterPeriodicSyncDidStore(std::move(callback),
                                   blink::ServiceWorkerStatusCode::kOk);
    return;
  }

  RemoveActiveRegistration(std::move(registration_info));
  StoreRegistrations(
      sw_registration_id,
      base::BindOnce(&BackgroundSyncManager::UnregisterPeriodicSyncDidStore,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncManager::UnregisterPeriodicSyncDidStore(
    BackgroundSyncManager::StatusCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    BackgroundSyncMetrics::CountUnregisterPeriodicSync(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    DisableAndClearManager(base::BindOnce(
        std::move(callback), BACKGROUND_SYNC_STATUS_STORAGE_ERROR));
    return;
  }

  BackgroundSyncMetrics::CountUnregisterPeriodicSync(BACKGROUND_SYNC_STATUS_OK);
  ScheduleOrCancelDelayedProcessing(BackgroundSyncType::PERIODIC);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), BACKGROUND_SYNC_STATUS_OK));
}

void BackgroundSyncManager::DisableAndClearManager(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  disabled_ = true;

  active_registrations_.clear();

  // Delete all backend entries. The memory representation of registered syncs
  // may be out of sync with storage (e.g., due to corruption detection on
  // loading from storage), so reload the registrations from storage again.
  GetDataFromBackend(
      kBackgroundSyncUserDataKey,
      base::BindOnce(&BackgroundSyncManager::DisableAndClearDidGetRegistrations,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncManager::DisableAndClearDidGetRegistrations(
    base::OnceClosure callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != blink::ServiceWorkerStatusCode::kOk || user_data.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(user_data.size(), std::move(callback));

  for (const auto& sw_id_and_regs : user_data) {
    service_worker_context_->ClearRegistrationUserData(
        sw_id_and_regs.first, {kBackgroundSyncUserDataKey},
        base::BindOnce(&BackgroundSyncManager::DisableAndClearManagerClearedOne,
                       weak_ptr_factory_.GetWeakPtr(), barrier_closure));
  }
}

void BackgroundSyncManager::DisableAndClearManagerClearedOne(
    base::OnceClosure barrier_closure,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The status doesn't matter at this point, there is nothing else to be done.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(barrier_closure));
}

BackgroundSyncRegistration* BackgroundSyncManager::LookupActiveRegistration(
    const blink::mojom::BackgroundSyncRegistrationInfo& registration_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = active_registrations_.find(
      registration_info.service_worker_registration_id);
  if (it == active_registrations_.end())
    return nullptr;

  BackgroundSyncRegistrations& registrations = it->second;
  DCHECK(!registrations.origin.opaque());

  auto key_and_registration_iter = registrations.registration_map.find(
      {registration_info.tag, registration_info.sync_type});
  if (key_and_registration_iter == registrations.registration_map.end())
    return nullptr;

  return &key_and_registration_iter->second;
}

void BackgroundSyncManager::StoreRegistrations(
    int64_t sw_registration_id,
    ServiceWorkerRegistry::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Serialize the data.
  const BackgroundSyncRegistrations& registrations =
      active_registrations_[sw_registration_id];
  BackgroundSyncRegistrationsProto registrations_proto;
  registrations_proto.set_origin(registrations.origin.Serialize());

  for (const auto& key_and_registration : registrations.registration_map) {
    const BackgroundSyncRegistration& registration =
        key_and_registration.second;
    BackgroundSyncRegistrationProto* registration_proto =
        registrations_proto.add_registration();
    registration_proto->set_tag(registration.options()->tag);
    if (registration.options()->min_interval >= 0) {
      registration_proto->mutable_periodic_sync_options()->set_min_interval(
          registration.options()->min_interval);
    }
    registration_proto->set_num_attempts(registration.num_attempts());
    registration_proto->set_max_attempts(registration.max_attempts());
    registration_proto->set_delay_until(
        registration.delay_until().ToInternalValue());
  }
  std::string serialized;
  bool success = registrations_proto.SerializeToString(&serialized);
  DCHECK(success);

  StoreDataInBackend(sw_registration_id, registrations.origin,
                     kBackgroundSyncUserDataKey, serialized,
                     std::move(callback));
}

void BackgroundSyncManager::RegisterDidStore(
    int64_t sw_registration_id,
    const BackgroundSyncRegistration& registration,
    StatusAndRegistrationCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // The service worker registration is gone.
    active_registrations_.erase(sw_registration_id);
    RecordFailureAndPostError(registration.sync_type(),
                              BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              std::move(callback));
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    BackgroundSyncMetrics::CountRegisterFailure(
        registration.sync_type(), BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    DisableAndClearManager(base::BindOnce(
        std::move(callback), BACKGROUND_SYNC_STATUS_STORAGE_ERROR, nullptr));
    return;
  }

  // Update controller of this new origin.
  if (registration.sync_type() == BackgroundSyncType::PERIODIC)
    proxy_->AddToTrackedOrigins(registration.origin());

  BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
      AreOptionConditionsMet()
          ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
          : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;
  BackgroundSyncMetrics::CountRegisterSuccess(
      registration.sync_type(), registration.options()->min_interval,
      registration_could_fire,
      BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE);

  ScheduleOrCancelDelayedProcessing(BackgroundSyncType::PERIODIC);

  // Tell the client that the registration is ready. We won't fire it until the
  // client has resolved the registration event.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), BACKGROUND_SYNC_STATUS_OK,
                                std::make_unique<BackgroundSyncRegistration>(
                                    registration)));
}

void BackgroundSyncManager::DidResolveRegistrationImpl(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BackgroundSyncRegistration* registration =
      LookupActiveRegistration(*registration_info);
  if (!registration) {
    // There might not be a registration if the client ack's a registration that
    // was a duplicate in the first place and was already firing and finished by
    // the time the client acknowledged the second registration.
    op_scheduler_.CompleteOperationAndRunNext();
    return;
  }

  registration->set_resolved();

  ResolveRegistrationDidCreateKeepAlive(CreateBackgroundSyncEventKeepAlive(
      service_worker_context_, std::move(*registration_info)));
}

void BackgroundSyncManager::ResolveRegistrationDidCreateKeepAlive(
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  FireReadyEvents(BackgroundSyncType::ONE_SHOT, /* reschedule= */ true,
                  base::DoNothing(), std::move(keepalive));
  op_scheduler_.CompleteOperationAndRunNext();
}

void BackgroundSyncManager::RemoveActiveRegistration(
    const blink::mojom::BackgroundSyncRegistrationInfo& registration_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(LookupActiveRegistration(registration_info));

  BackgroundSyncRegistrations* registrations =
      &active_registrations_[registration_info.service_worker_registration_id];
  const url::Origin& origin = registrations->origin;

  registrations->registration_map.erase(
      {registration_info.tag, registration_info.sync_type});

  // Update controller's list of registered origin if necessary.
  if (registrations->registration_map.empty())
    proxy_->RemoveFromTrackedOrigins(origin);
  else {
    bool no_more_periodic_sync_registrations = true;
    for (auto& key_and_registration : registrations->registration_map) {
      if (key_and_registration.second.sync_type() ==
          BackgroundSyncType::PERIODIC) {
        no_more_periodic_sync_registrations = false;
        break;
      }
    }
    if (no_more_periodic_sync_registrations)
      proxy_->RemoveFromTrackedOrigins(origin);
  }

  if (registration_info.sync_type == BackgroundSyncType::PERIODIC &&
      ShouldLogToDevTools(registration_info.sync_type)) {
    devtools_context_->LogBackgroundServiceEvent(
        registration_info.service_worker_registration_id,
        blink::StorageKey::CreateFirstParty(origin),
        DevToolsBackgroundService::kPeriodicBackgroundSync,
        /* event_name= */ "Unregistered periodicsync",
        /* instance_id= */ registration_info.tag,
        /* event_metadata= */ {});
  }
}

void BackgroundSyncManager::AddOrUpdateActiveRegistration(
    int64_t sw_registration_id,
    const url::Origin& origin,
    const BackgroundSyncRegistration& sync_registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BackgroundSyncRegistrations* registrations =
      &active_registrations_[sw_registration_id];
  registrations->origin = origin;

  BackgroundSyncType sync_type = sync_registration.sync_type();
  registrations
      ->registration_map[{sync_registration.options()->tag, sync_type}] =
      sync_registration;

  if (ShouldLogToDevTools(sync_registration.sync_type())) {
    std::map<std::string, std::string> event_metadata;
    if (sync_registration.sync_type() == BackgroundSyncType::PERIODIC) {
      event_metadata["minInterval"] =
          base::NumberToString(sync_registration.options()->min_interval);
    }
    devtools_context_->LogBackgroundServiceEvent(
        sw_registration_id, blink::StorageKey::CreateFirstParty(origin),
        GetDevToolsBackgroundService(sync_type),
        /* event_name= */ "Registered " + GetSyncEventName(sync_type),
        /* instance_id= */ sync_registration.options()->tag, event_metadata);
  }
}

void BackgroundSyncManager::StoreDataInBackend(
    int64_t sw_registration_id,
    const url::Origin& origin,
    const std::string& backend_key,
    const std::string& data,
    ServiceWorkerRegistry::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_worker_context_->StoreRegistrationUserData(
      sw_registration_id, blink::StorageKey::CreateFirstParty(origin),
      {{backend_key, data}}, std::move(callback));
}

void BackgroundSyncManager::GetDataFromBackend(
    const std::string& backend_key,
    ServiceWorkerRegistry::GetUserDataForAllRegistrationsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  service_worker_context_->GetUserDataForAllRegistrations(backend_key,
                                                          std::move(callback));
}

void BackgroundSyncManager::DispatchSyncEvent(
    const std::string& tag,
    scoped_refptr<ServiceWorkerVersion> active_version,
    bool last_chance,
    ServiceWorkerVersion::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(active_version);

  if (active_version->running_status() !=
      blink::EmbeddedWorkerStatus::kRunning) {
    active_version->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::SYNC,
        base::BindOnce(&DidStartWorkerForSyncEvent,
                       base::BindOnce(&BackgroundSyncManager::DispatchSyncEvent,
                                      weak_ptr_factory_.GetWeakPtr(), tag,
                                      active_version, last_chance),
                       std::move(callback)));
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  int request_id = active_version->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC, std::move(split_callback.first),
      parameters_->max_sync_event_duration,
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);

  active_version->endpoint()->DispatchSyncEvent(
      tag, last_chance, parameters_->max_sync_event_duration,
      base::BindOnce(&OnSyncEventFinished, active_version, request_id,
                     std::move(split_callback.second)));

  if (devtools_context_->IsRecording(
          DevToolsBackgroundService::kBackgroundSync)) {
    devtools_context_->LogBackgroundServiceEvent(
        active_version->registration_id(), active_version->key(),
        DevToolsBackgroundService::kBackgroundSync,
        /* event_name= */ "Dispatched sync event",
        /* instance_id= */ tag,
        /* event_metadata= */
        {{"Last Chance", last_chance ? "Yes" : "No"}});
  }
}

void BackgroundSyncManager::DispatchPeriodicSyncEvent(
    const std::string& tag,
    scoped_refptr<ServiceWorkerVersion> active_version,
    ServiceWorkerVersion::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(active_version);

  if (active_version->running_status() !=
      blink::EmbeddedWorkerStatus::kRunning) {
    active_version->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::PERIODIC_SYNC,
        base::BindOnce(
            &DidStartWorkerForSyncEvent,
            base::BindOnce(&BackgroundSyncManager::DispatchPeriodicSyncEvent,
                           weak_ptr_factory_.GetWeakPtr(), tag, active_version),
            std::move(callback)));
    return;
  }

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  int request_id = active_version->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::PERIODIC_SYNC,
      std::move(split_callback.first), parameters_->max_sync_event_duration,
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);

  active_version->endpoint()->DispatchPeriodicSyncEvent(
      tag, parameters_->max_sync_event_duration,
      base::BindOnce(&OnSyncEventFinished, active_version, request_id,
                     std::move(split_callback.second)));

  if (devtools_context_->IsRecording(
          DevToolsBackgroundService::kPeriodicBackgroundSync)) {
    devtools_context_->LogBackgroundServiceEvent(
        active_version->registration_id(), active_version->key(),
        DevToolsBackgroundService::kPeriodicBackgroundSync,
        /* event_name= */ "Dispatched periodicsync event",
        /* instance_id= */ tag,
        /* event_metadata= */ {});
  }
}

void BackgroundSyncManager::HasMainFrameWindowClient(
    const blink::StorageKey& key,
    BoolCallback callback) {
  service_worker_context_->HasMainFrameWindowClient(key, std::move(callback));
}

void BackgroundSyncManager::GetRegistrationsImpl(
    BackgroundSyncType sync_type,
    int64_t sw_registration_id,
    StatusAndRegistrationsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<std::unique_ptr<BackgroundSyncRegistration>> out_registrations;

  if (disabled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                                  std::move(out_registrations)));
    return;
  }

  auto it = active_registrations_.find(sw_registration_id);

  if (it != active_registrations_.end()) {
    const BackgroundSyncRegistrations& registrations = it->second;
    for (const auto& key_and_registration : registrations.registration_map) {
      const BackgroundSyncRegistration& registration =
          key_and_registration.second;
      if (registration.sync_type() != sync_type)
        continue;
      out_registrations.push_back(
          std::make_unique<BackgroundSyncRegistration>(registration));
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), BACKGROUND_SYNC_STATUS_OK,
                                std::move(out_registrations)));
}

bool BackgroundSyncManager::AreOptionConditionsMet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return network_observer_->NetworkSufficient();
}

bool BackgroundSyncManager::AllConditionsExceptConnectivitySatisfied(
    const BackgroundSyncRegistration& registration,
    int64_t service_worker_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Don't fire the registration if the client hasn't yet resolved its
  // registration promise.
  if (!registration.resolved() &&
      registration.sync_type() == BackgroundSyncType::ONE_SHOT) {
    return false;
  }

  if (registration.sync_state() != blink::mojom::BackgroundSyncState::PENDING)
    return false;

  if (registration.is_suspended())
    return false;

  if (base::Contains(emulated_offline_sw_, service_worker_id))
    return false;

  return true;
}

bool BackgroundSyncManager::CanFireAnyRegistrationUponConnectivity(
    BackgroundSyncType sync_type) {
  for (const auto& sw_reg_id_and_registrations : active_registrations_) {
    int64_t service_worker_registration_id = sw_reg_id_and_registrations.first;
    for (const auto& key_and_registration :
         sw_reg_id_and_registrations.second.registration_map) {
      const BackgroundSyncRegistration& registration =
          key_and_registration.second;
      if (sync_type != registration.sync_type())
        continue;

      if (AllConditionsExceptConnectivitySatisfied(
              registration, service_worker_registration_id)) {
        return true;
      }
    }
  }
  return false;
}

bool& BackgroundSyncManager::delayed_processing_scheduled(
    BackgroundSyncType sync_type) {
  if (sync_type == BackgroundSyncType::ONE_SHOT)
    return delayed_processing_scheduled_one_shot_sync_;
  else
    return delayed_processing_scheduled_periodic_sync_;
}

void BackgroundSyncManager::ScheduleOrCancelDelayedProcessing(
    BackgroundSyncType sync_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool can_fire_with_connectivity =
      CanFireAnyRegistrationUponConnectivity(sync_type);

  if (delayed_processing_scheduled(sync_type) && !can_fire_with_connectivity &&
      !GetNumFiringRegistrations(sync_type)) {
    CancelDelayedProcessingOfRegistrations(sync_type);
    delayed_processing_scheduled(sync_type) = false;
  } else if (can_fire_with_connectivity ||
             GetNumFiringRegistrations(sync_type)) {
    ScheduleDelayedProcessingOfRegistrations(sync_type);
    delayed_processing_scheduled(sync_type) = true;
  }
}

bool BackgroundSyncManager::IsRegistrationReadyToFire(
    const BackgroundSyncRegistration& registration,
    int64_t service_worker_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (clock_->Now() < registration.delay_until())
    return false;

  return AllConditionsExceptConnectivitySatisfied(registration,
                                                  service_worker_id) &&
         AreOptionConditionsMet();
}

int BackgroundSyncManager::GetNumFiringRegistrations(
    BackgroundSyncType sync_type) {
  if (sync_type == BackgroundSyncType::ONE_SHOT)
    return num_firing_registrations_one_shot_;
  return num_firing_registrations_periodic_;
}

void BackgroundSyncManager::UpdateNumFiringRegistrationsBy(
    BackgroundSyncType sync_type,
    int to_add) {
  if (sync_type == BackgroundSyncType::ONE_SHOT)
    num_firing_registrations_one_shot_ += to_add;
  else
    num_firing_registrations_periodic_ += to_add;
}

bool BackgroundSyncManager::AllRegistrationsWaitingToBeResolved() const {
  for (const auto& active_registration : active_registrations_) {
    for (const auto& key_and_registration :
         active_registration.second.registration_map) {
      const BackgroundSyncRegistration& registration =
          key_and_registration.second;
      if (registration.resolved())
        return false;
    }
  }
  return true;
}

base::TimeDelta BackgroundSyncManager::GetSoonestWakeupDelta(
    BackgroundSyncType sync_type,
    base::Time last_browser_wakeup_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeDelta soonest_wakeup_delta = base::TimeDelta::Max();
  bool need_retries = false;
  for (const auto& sw_reg_id_and_registrations : active_registrations_) {
    for (const auto& key_and_registration :
         sw_reg_id_and_registrations.second.registration_map) {
      const BackgroundSyncRegistration& registration =
          key_and_registration.second;
      if (registration.sync_type() != sync_type)
        continue;
      if (registration.num_attempts() > 0 &&
          registration.num_attempts() < registration.max_attempts()) {
        need_retries = true;
      }
      if (registration.sync_state() ==
          blink::mojom::BackgroundSyncState::PENDING) {
        if (clock_->Now() >= registration.delay_until()) {
          soonest_wakeup_delta = base::TimeDelta();
          break;
        } else {
          base::TimeDelta delay_delta =
              registration.delay_until() - clock_->Now();
          soonest_wakeup_delta = std::min(delay_delta, soonest_wakeup_delta);
        }
      }
    }
  }

  // If the browser is closed while firing events, the browser needs a task to
  // wake it back up and try again.
  if (GetNumFiringRegistrations(sync_type) > 0 &&
      soonest_wakeup_delta > parameters_->min_sync_recovery_time) {
    soonest_wakeup_delta = parameters_->min_sync_recovery_time;
  }

  // If we're still waiting for registrations to be resolved, don't schedule
  // a wake up task eagerly.
  if (sync_type == BackgroundSyncType::ONE_SHOT &&
      AllRegistrationsWaitingToBeResolved() &&
      soonest_wakeup_delta < parameters_->min_sync_recovery_time) {
    soonest_wakeup_delta = parameters_->min_sync_recovery_time;
  }

  // The browser may impose a hard limit on how often it can be woken up to
  // process periodic Background Sync registrations. This excludes retries.
  if (sync_type == BackgroundSyncType::PERIODIC && !need_retries) {
    soonest_wakeup_delta = MaybeApplyBrowserWakeupCountLimit(
        soonest_wakeup_delta, last_browser_wakeup_time);
  }
  return soonest_wakeup_delta;
}

base::TimeDelta BackgroundSyncManager::MaybeApplyBrowserWakeupCountLimit(
    base::TimeDelta soonest_wakeup_delta,
    base::Time last_browser_wakeup_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (last_browser_wakeup_time.is_null())
    return soonest_wakeup_delta;

  base::TimeDelta time_since_last_browser_wakeup =
      clock_->Now() - last_browser_wakeup_time;
  if (time_since_last_browser_wakeup >=
      parameters_->min_periodic_sync_events_interval) {
    return soonest_wakeup_delta;
  }

  base::TimeDelta time_till_next_allowed_browser_wakeup =
      parameters_->min_periodic_sync_events_interval -
      time_since_last_browser_wakeup;
  return std::max(soonest_wakeup_delta, time_till_next_allowed_browser_wakeup);
}

base::TimeDelta
BackgroundSyncManager::GetSmallestPeriodicSyncEventDelayForOrigin(
    const url::Origin& origin,
    const std::string& tag_to_skip) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Time soonest_wakeup_time = base::Time();
  for (const auto& active_registration : active_registrations_) {
    if (active_registration.second.origin != origin)
      continue;

    const auto& tag_and_registrations =
        active_registration.second.registration_map;
    for (const auto& tag_and_registration : tag_and_registrations) {
      if (/* tag= */ tag_and_registration.first.first == tag_to_skip)
        continue;
      if (/* sync_type= */ tag_and_registration.first.second !=
          BackgroundSyncType::PERIODIC) {
        continue;
      }
      if (tag_and_registration.second.delay_until().is_null())
        continue;
      if (soonest_wakeup_time.is_null() ||
          tag_and_registration.second.delay_until() < soonest_wakeup_time) {
        soonest_wakeup_time = tag_and_registration.second.delay_until();
      }
    }
  }

  if (soonest_wakeup_time.is_null())
    return base::TimeDelta::Max();

  if (soonest_wakeup_time < clock_->Now())
    return base::TimeDelta();

  return soonest_wakeup_time - clock_->Now();
}

void BackgroundSyncManager::RevivePeriodicSyncRegistrations(
    url::Origin origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_)
    return;

  op_scheduler_.ScheduleOperation(base::BindOnce(
      &BackgroundSyncManager::ReviveOriginImpl, weak_ptr_factory_.GetWeakPtr(),
      std::move(origin), MakeEmptyCompletion()));
}

void BackgroundSyncManager::ReviveOriginImpl(url::Origin origin,
                                             base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  // Create a list of registrations to revive.
  std::vector<const BackgroundSyncRegistration*> to_revive;
  std::map<const BackgroundSyncRegistration*, int64_t>
      service_worker_registration_ids;

  for (const auto& active_registration : active_registrations_) {
    int64_t service_worker_registration_id = active_registration.first;
    if (active_registration.second.origin != origin)
      continue;

    for (const auto& key_and_registration :
         active_registration.second.registration_map) {
      const BackgroundSyncRegistration* registration =
          &key_and_registration.second;
      if (!registration->is_suspended())
        continue;

      to_revive.push_back(registration);
      service_worker_registration_ids[registration] =
          service_worker_registration_id;
    }
  }

  if (to_revive.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  base::RepeatingClosure received_new_delays_closure = base::BarrierClosure(
      to_revive.size(),
      base::BindOnce(
          &BackgroundSyncManager::DidReceiveDelaysForSuspendedRegistrations,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  for (const auto* registration : to_revive) {
    base::TimeDelta delay = GetNextEventDelay(
        service_worker_context_, *registration,
        std::make_unique<BackgroundSyncParameters>(*parameters_),
        GetSmallestPeriodicSyncEventDelayForOrigin(
            origin, registration->options()->tag));
    ReviveDidGetNextEventDelay(service_worker_registration_ids[registration],
                               *registration, received_new_delays_closure,
                               delay);
  }
}

void BackgroundSyncManager::ReviveDidGetNextEventDelay(
    int64_t service_worker_registration_id,
    BackgroundSyncRegistration registration,
    base::OnceClosure done_closure,
    base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (delay.is_max()) {
    std::move(done_closure).Run();
    return;
  }

  BackgroundSyncRegistration* active_registration =
      LookupActiveRegistration(blink::mojom::BackgroundSyncRegistrationInfo(
          service_worker_registration_id, registration.options()->tag,
          registration.sync_type()));
  if (!active_registration) {
    std::move(done_closure).Run();
    return;
  }

  active_registration->set_delay_until(clock_->Now() + delay);

  StoreRegistrations(
      service_worker_registration_id,
      base::BindOnce(&BackgroundSyncManager::ReviveDidStoreRegistration,
                     weak_ptr_factory_.GetWeakPtr(),
                     service_worker_registration_id, std::move(done_closure)));
}

void BackgroundSyncManager::ReviveDidStoreRegistration(
    int64_t service_worker_registration_id,
    base::OnceClosure done_closure,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // The service worker registration is gone.
    active_registrations_.erase(service_worker_registration_id);
    std::move(done_closure).Run();
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    DisableAndClearManager(std::move(done_closure));
    return;
  }

  std::move(done_closure).Run();
}

void BackgroundSyncManager::DidReceiveDelaysForSuspendedRegistrations(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScheduleOrCancelDelayedProcessing(BackgroundSyncType::PERIODIC);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void BackgroundSyncManager::ScheduleDelayedProcessingOfRegistrations(
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto fire_events_callback = base::BindOnce(
      &BackgroundSyncManager::FireReadyEvents, weak_ptr_factory_.GetWeakPtr(),
      sync_type, /* reschedule= */ true, base::DoNothing(),
      /* keepalive= */ nullptr);

  proxy_->ScheduleDelayedProcessing(
      sync_type,
      GetSoonestWakeupDelta(sync_type,
                            /* last_browser_wakeup_time= */ base::Time()),
      std::move(fire_events_callback));
}

void BackgroundSyncManager::CancelDelayedProcessingOfRegistrations(
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  proxy_->CancelDelayedProcessing(sync_type);
}

void BackgroundSyncManager::FireReadyEvents(
    blink::mojom::BackgroundSyncType sync_type,
    bool reschedule,
    base::OnceClosure callback,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!reschedule) {
    // This invocation has come from scheduled processing of registrations.
    // Since this delayed processing is one-off, update internal state.
    delayed_processing_scheduled(sync_type) = false;
  }

  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::FireReadyEventsImpl,
                     weak_ptr_factory_.GetWeakPtr(), sync_type, reschedule,
                     std::move(callback), std::move(keepalive)));
}

void BackgroundSyncManager::FireReadyEventsImpl(
    blink::mojom::BackgroundSyncType sync_type,
    bool reschedule,
    base::OnceClosure callback,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, op_scheduler_.WrapCallbackToRunNext(std::move(callback)));
    return;
  }

  // Find the registrations that are ready to run.
  std::vector<blink::mojom::BackgroundSyncRegistrationInfoPtr> to_fire;

  for (auto& sw_reg_id_and_registrations : active_registrations_) {
    const int64_t service_worker_registration_id =
        sw_reg_id_and_registrations.first;
    for (auto& key_and_registration :
         sw_reg_id_and_registrations.second.registration_map) {
      BackgroundSyncRegistration* registration = &key_and_registration.second;
      if (sync_type != registration->sync_type())
        continue;

      if (IsRegistrationReadyToFire(*registration,
                                    service_worker_registration_id)) {
        to_fire.emplace_back(blink::mojom::BackgroundSyncRegistrationInfo::New(
            service_worker_registration_id,
            /* tag= */ key_and_registration.first.first,
            /* sync_type= */ key_and_registration.first.second));

        // The state change is not saved to persistent storage because
        // if the sync event is killed mid-sync then it should return to
        // SYNC_STATE_PENDING.
        registration->set_sync_state(blink::mojom::BackgroundSyncState::FIRING);
      }
    }
  }

  if (!reschedule) {
    // This method has been called from a Chrome wakeup task.
    BackgroundSyncMetrics::RecordEventsFiredFromWakeupTask(
        sync_type, /* events_fired= */ !to_fire.empty());
  }

  if (to_fire.empty()) {
    // TODO(crbug.com/40641360): Reschedule wakeup after a non-zero delay if
    // called from a wakeup task.
    if (reschedule)
      ScheduleOrCancelDelayedProcessing(sync_type);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, op_scheduler_.WrapCallbackToRunNext(std::move(callback)));
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();

  // If we've been called from a wake up task, potentially keep the browser
  // awake till all events have completed. If not, we only do so until all
  // events have been fired.
  // To allow the |op_scheduler_| to process other tasks after sync events
  // have been fired, mark this task complete after firing events.
  base::OnceClosure events_fired_callback, events_completed_callback;
  bool keep_browser_awake_till_events_complete =
      !reschedule && parameters_->keep_browser_awake_till_events_complete;
  if (keep_browser_awake_till_events_complete) {
    events_fired_callback = MakeEmptyCompletion();
    events_completed_callback = std::move(callback);
  } else {
    events_fired_callback =
        op_scheduler_.WrapCallbackToRunNext(std::move(callback));
    events_completed_callback = base::DoNothing();
  }

  // Fire the sync event of the ready registrations and run
  // |events_fired_closure| once they're all done.
  base::RepeatingClosure events_fired_barrier_closure = base::BarrierClosure(
      to_fire.size(),
      base::BindOnce(&BackgroundSyncManager::FireReadyEventsAllEventsFiring,
                     weak_ptr_factory_.GetWeakPtr(), sync_type, reschedule,
                     std::move(events_fired_callback)));

  // Record the total time taken after all events have run to completion.
  base::RepeatingClosure events_completed_barrier_closure =
      base::BarrierClosure(
          to_fire.size(),
          base::BindOnce(&BackgroundSyncManager::OnAllSyncEventsCompleted,
                         sync_type, start_time, !reschedule, to_fire.size(),
                         std::move(events_completed_callback)));

  for (auto& registration_info : to_fire) {
    const BackgroundSyncRegistration* registration =
        LookupActiveRegistration(*registration_info);
    DCHECK(registration);

    int64_t service_worker_registration_id =
        registration_info->service_worker_registration_id;
    // If BackgroundSync becomes usable from a 3p context then
    // BackgroundSyncRegistrations should be changed to use StorageKey.
    service_worker_context_->FindReadyRegistrationForId(
        service_worker_registration_id,
        blink::StorageKey::CreateFirstParty(
            active_registrations_[service_worker_registration_id].origin),
        base::BindOnce(
            &BackgroundSyncManager::FireReadyEventsDidFindRegistration,
            weak_ptr_factory_.GetWeakPtr(), std::move(registration_info),
            std::move(keepalive), events_fired_barrier_closure,
            events_completed_barrier_closure));
  }
}

void BackgroundSyncManager::FireReadyEventsDidFindRegistration(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive,
    base::OnceClosure event_fired_callback,
    base::OnceClosure event_completed_callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BackgroundSyncRegistration* registration =
      LookupActiveRegistration(*registration_info);

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    if (registration)
      registration->set_sync_state(blink::mojom::BackgroundSyncState::PENDING);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(event_fired_callback));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(event_completed_callback));
    return;
  }

  DCHECK_EQ(registration_info->service_worker_registration_id,
            service_worker_registration->id());
  DCHECK(registration);

  // The connectivity was lost before dispatching the sync event, so there is
  // no point in going through with it.
  if (!AreOptionConditionsMet()) {
    registration->set_sync_state(blink::mojom::BackgroundSyncState::PENDING);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(event_fired_callback));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(event_completed_callback));
    return;
  }

  auto sync_type = registration_info->sync_type;
  UpdateNumFiringRegistrationsBy(sync_type, 1);

  const bool last_chance =
      registration->num_attempts() == registration->max_attempts() - 1;

  HasMainFrameWindowClient(
      service_worker_registration->key(),
      base::BindOnce(&BackgroundSyncMetrics::RecordEventStarted, sync_type));

  if (sync_type == BackgroundSyncType::ONE_SHOT) {
    DispatchSyncEvent(
        registration->options()->tag,
        service_worker_registration->active_version(), last_chance,
        base::BindOnce(&BackgroundSyncManager::EventComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       service_worker_registration,
                       std::move(registration_info), std::move(keepalive),
                       std::move(event_completed_callback)));
  } else {
    DispatchPeriodicSyncEvent(
        registration->options()->tag,
        service_worker_registration->active_version(),
        base::BindOnce(&BackgroundSyncManager::EventComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       service_worker_registration,
                       std::move(registration_info), std::move(keepalive),
                       std::move(event_completed_callback)));
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(event_fired_callback));
}

void BackgroundSyncManager::FireReadyEventsAllEventsFiring(
    BackgroundSyncType sync_type,
    bool reschedule,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (reschedule)
    ScheduleOrCancelDelayedProcessing(sync_type);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

// |service_worker_registration| is just to keep the registration alive
// while the event is firing.
void BackgroundSyncManager::EventComplete(
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration,
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive,
    base::OnceClosure callback,
    blink::ServiceWorkerStatusCode status_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  // The event ran to completion, we should count it, no matter what happens
  // from here.
  const blink::StorageKey& key = service_worker_registration->key();
  HasMainFrameWindowClient(
      key, base::BindOnce(&BackgroundSyncMetrics::RecordEventResult,
                          registration_info->sync_type,
                          status_code == blink::ServiceWorkerStatusCode::kOk));

  op_scheduler_.ScheduleOperation(base::BindOnce(
      &BackgroundSyncManager::EventCompleteImpl, weak_ptr_factory_.GetWeakPtr(),
      std::move(registration_info), std::move(keepalive), status_code,
      key.origin(), op_scheduler_.WrapCallbackToRunNext(std::move(callback))));
}

void BackgroundSyncManager::EventCompleteImpl(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive,
    blink::ServiceWorkerStatusCode status_code,
    const url::Origin& origin,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (disabled_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  BackgroundSyncRegistration* registration =
      LookupActiveRegistration(*registration_info);
  if (!registration) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }
  DCHECK_NE(blink::mojom::BackgroundSyncState::PENDING,
            registration->sync_state());

  // It's important to update |num_attempts| before we update |delay_until|.
  bool succeeded = status_code == blink::ServiceWorkerStatusCode::kOk;
  registration->set_num_attempts(GetNumAttemptsAfterEvent(
      registration->sync_type(), registration->num_attempts(),
      registration->max_attempts(), registration->sync_state(), succeeded));

  // If |delay_until| needs to be updated, get updated delay.
  if (registration->sync_type() == BackgroundSyncType::PERIODIC ||
      (!succeeded &&
       registration->num_attempts() < registration->max_attempts())) {
    base::TimeDelta delay = GetNextEventDelay(
        service_worker_context_, *registration,
        std::make_unique<BackgroundSyncParameters>(*parameters_),
        GetSmallestPeriodicSyncEventDelayForOrigin(
            origin, registration->options()->tag));
    EventCompleteDidGetDelay(std::move(registration_info), status_code, origin,
                             std::move(callback), delay);
    return;
  }

  EventCompleteDidGetDelay(std::move(registration_info), status_code, origin,
                           std::move(callback), base::TimeDelta::Max());
}

void BackgroundSyncManager::EventCompleteDidGetDelay(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
    blink::ServiceWorkerStatusCode status_code,
    const url::Origin& origin,
    base::OnceClosure callback,
    base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateNumFiringRegistrationsBy(registration_info->sync_type, -1);

  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(origin);
  BackgroundSyncRegistration* registration =
      LookupActiveRegistration(*registration_info);
  if (!registration) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  bool succeeded = status_code == blink::ServiceWorkerStatusCode::kOk;
  bool can_retry = registration->num_attempts() < registration->max_attempts();

  bool registration_completed = true;
  if (registration->sync_state() ==
      blink::mojom::BackgroundSyncState::REREGISTERED_WHILE_FIRING) {
    registration->set_sync_state(blink::mojom::BackgroundSyncState::PENDING);
    registration->set_num_attempts(0);
    registration_completed = false;
    if (ShouldLogToDevTools(registration->sync_type())) {
      devtools_context_->LogBackgroundServiceEvent(
          registration_info->service_worker_registration_id, storage_key,
          GetDevToolsBackgroundService(registration->sync_type()),
          /* event_name= */ "Sync event reregistered",
          /* instance_id= */ registration_info->tag,
          /* event_metadata= */ {});
    }
  } else if ((!succeeded && can_retry) ||
             registration->sync_type() == BackgroundSyncType::PERIODIC) {
    registration->set_sync_state(blink::mojom::BackgroundSyncState::PENDING);
    registration_completed = false;
    registration->set_delay_until(clock_->Now() + delay);

    std::string event_name = GetSyncEventName(registration->sync_type()) +
                             (succeeded ? " event completed" : " event failed");
    base::TimeDelta display_delay =
        registration->sync_type() == BackgroundSyncType::ONE_SHOT
            ? delay
            : registration->delay_until() - clock_->Now();
    std::map<std::string, std::string> event_metadata = {
        {"Next Attempt Delay (ms)", GetDelayAsString(display_delay)}};
    if (!succeeded) {
      event_metadata.emplace("Failure Reason",
                             GetEventStatusString(status_code));
    }

    if (ShouldLogToDevTools(registration->sync_type())) {
      devtools_context_->LogBackgroundServiceEvent(
          registration_info->service_worker_registration_id, storage_key,
          GetDevToolsBackgroundService(registration->sync_type()), event_name,
          /* instance_id= */ registration_info->tag, event_metadata);
    }
  }

  if (registration_completed) {
    BackgroundSyncMetrics::RecordRegistrationComplete(
        succeeded, registration->num_attempts());

    if (ShouldLogToDevTools(registration->sync_type())) {
      devtools_context_->LogBackgroundServiceEvent(
          registration_info->service_worker_registration_id, storage_key,
          GetDevToolsBackgroundService(registration->sync_type()),
          /* event_name= */ "Sync completed",
          /* instance_id= */ registration_info->tag,
          {{"Status", GetEventStatusString(status_code)}});
    }

    if (registration_info->sync_type ==
        blink::mojom::BackgroundSyncType::ONE_SHOT) {
      NotifyOneShotBackgroundSyncCompleted(
          service_worker_context_, origin, status_code,
          registration->num_attempts(), registration->max_attempts());
    } else {
      NotifyPeriodicBackgroundSyncCompleted(
          service_worker_context_, origin, status_code,
          registration->num_attempts(), registration->max_attempts());
    }

    RemoveActiveRegistration(*registration_info);
  }

  StoreRegistrations(
      registration_info->service_worker_registration_id,
      base::BindOnce(&BackgroundSyncManager::EventCompleteDidStore,
                     weak_ptr_factory_.GetWeakPtr(),
                     registration_info->sync_type,
                     registration_info->service_worker_registration_id,
                     std::move(callback)));
}

void BackgroundSyncManager::EventCompleteDidStore(
    blink::mojom::BackgroundSyncType sync_type,
    int64_t service_worker_id,
    base::OnceClosure callback,
    blink::ServiceWorkerStatusCode status_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status_code == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // The registration is gone.
    active_registrations_.erase(service_worker_id);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }

  if (status_code != blink::ServiceWorkerStatusCode::kOk) {
    DisableAndClearManager(std::move(callback));
    return;
  }

  // Fire any ready events and call RunInBackground if anything is waiting.
  FireReadyEvents(sync_type, /* reschedule= */ true, base::DoNothing());

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

// static
void BackgroundSyncManager::OnAllSyncEventsCompleted(
    BackgroundSyncType sync_type,
    const base::TimeTicks& start_time,
    bool from_wakeup_task,
    int number_of_batched_sync_events,
    base::OnceClosure callback) {
  // Record the combined time taken by all sync events.
  BackgroundSyncMetrics::RecordBatchSyncEventComplete(
      sync_type, base::TimeTicks::Now() - start_time, from_wakeup_task,
      number_of_batched_sync_events);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void BackgroundSyncManager::OnRegistrationDeletedImpl(
    int64_t sw_registration_id,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The backend (ServiceWorkerStorage) will delete the data, so just delete the
  // memory representation here.
  active_registrations_.erase(sw_registration_id);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void BackgroundSyncManager::OnStorageWipedImpl(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  active_registrations_.clear();
  disabled_ = false;
  InitImpl(std::move(callback));
}

void BackgroundSyncManager::OnNetworkChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID)
  if (parameters_->rely_on_android_network_detection)
    return;
#endif

  if (!AreOptionConditionsMet())
    return;

  FireReadyEvents(BackgroundSyncType::ONE_SHOT, /* reschedule= */ true,
                  base::DoNothing());
  FireReadyEvents(BackgroundSyncType::PERIODIC, /* reschedule= */ true,
                  base::DoNothing());
}

base::OnceClosure BackgroundSyncManager::MakeEmptyCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return op_scheduler_.WrapCallbackToRunNext(base::BindOnce([] {}));
}

blink::ServiceWorkerStatusCode BackgroundSyncManager::CanEmulateSyncEvent(
    scoped_refptr<ServiceWorkerVersion> active_version) {
  if (!active_version)
    return blink::ServiceWorkerStatusCode::kErrorAbort;
  if (!network_observer_->NetworkSufficient())
    return blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected;
  int64_t registration_id = active_version->registration_id();
  if (base::Contains(emulated_offline_sw_, registration_id))
    return blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected;
  return blink::ServiceWorkerStatusCode::kOk;
}

bool BackgroundSyncManager::ShouldLogToDevTools(BackgroundSyncType sync_type) {
  return devtools_context_->IsRecording(
      GetDevToolsBackgroundService(sync_type));
}

}  // namespace content
