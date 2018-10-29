// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_manager.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/background_sync/background_sync_metrics.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/background_sync/background_sync_registration_options.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"

#if defined(OS_ANDROID)
#include "content/browser/android/background_sync_network_observer_android.h"
#endif

namespace content {

namespace {

// The key used to index the background sync data in ServiceWorkerStorage.
const char kBackgroundSyncUserDataKey[] = "BackgroundSyncUserData";

void RecordFailureAndPostError(
    BackgroundSyncStatus status,
    BackgroundSyncManager::StatusAndRegistrationCallback callback) {
  BackgroundSyncMetrics::CountRegisterFailure(status);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status, nullptr));
}

// Returns nullptr if the browser context cannot be accessed for any reason.
BrowserContext* GetBrowserContextOnUIThread(
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
BackgroundSyncController* GetBackgroundSyncControllerOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      GetBrowserContextOnUIThread(std::move(service_worker_context));
  if (!browser_context)
    return nullptr;

  return browser_context->GetBackgroundSyncController();
}

blink::mojom::PermissionStatus GetBackgroundSyncPermissionOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      GetBrowserContextOnUIThread(std::move(service_worker_context));
  if (!browser_context)
    return blink::mojom::PermissionStatus::DENIED;

  PermissionController* permission_controller =
      BrowserContext::GetPermissionController(browser_context);
  DCHECK(permission_controller);

  // The requesting origin always matches the embedding origin.
  return permission_controller->GetPermissionStatus(
      PermissionType::BACKGROUND_SYNC, origin, origin);
}

void NotifyBackgroundSyncRegisteredOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncControllerOnUIThread(std::move(sw_context_wrapper));

  if (!background_sync_controller)
    return;

  background_sync_controller->NotifyBackgroundSyncRegistered(origin);
}

void RunInBackgroundOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    bool enabled,
    int64_t min_ms) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncControllerOnUIThread(sw_context_wrapper);
  if (background_sync_controller) {
    background_sync_controller->RunInBackground(enabled, min_ms);
  }
}

std::unique_ptr<BackgroundSyncParameters> GetControllerParameters(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    std::unique_ptr<BackgroundSyncParameters> parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncControllerOnUIThread(sw_context_wrapper);

  if (!background_sync_controller) {
    // If there is no controller then BackgroundSync can't run in the
    // background, disable it.
    parameters->disable = true;
    return parameters;
  }

  background_sync_controller->GetParameterOverrides(parameters.get());
  return parameters;
}

void OnSyncEventFinished(scoped_refptr<ServiceWorkerVersion> active_version,
                         int request_id,
                         ServiceWorkerVersion::StatusCallback callback,
                         blink::mojom::ServiceWorkerEventStatus status,
                         base::TimeTicks dispatch_event_time) {
  if (!active_version->FinishRequest(
          request_id,
          status == blink::mojom::ServiceWorkerEventStatus::COMPLETED,
          dispatch_event_time)) {
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

}  // namespace

BackgroundSyncManager::BackgroundSyncRegistrations::
    BackgroundSyncRegistrations()
    : next_id(BackgroundSyncRegistration::kInitialId) {
}

BackgroundSyncManager::BackgroundSyncRegistrations::BackgroundSyncRegistrations(
    const BackgroundSyncRegistrations& other) = default;

BackgroundSyncManager::BackgroundSyncRegistrations::
    ~BackgroundSyncRegistrations() {
}

// static
std::unique_ptr<BackgroundSyncManager> BackgroundSyncManager::Create(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncManager* sync_manager =
      new BackgroundSyncManager(service_worker_context);
  sync_manager->Init();
  return base::WrapUnique(sync_manager);
}

BackgroundSyncManager::~BackgroundSyncManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->RemoveObserver(this);
}

void BackgroundSyncManager::Register(
    int64_t sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    StatusAndRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              std::move(callback));
    return;
  }

  op_scheduler_.ScheduleOperation(base::BindOnce(
      &BackgroundSyncManager::RegisterCheckIfHasMainFrame,
      weak_ptr_factory_.GetWeakPtr(), sw_registration_id, options,
      op_scheduler_.WrapCallbackToRunNext(std::move(callback))));
}

void BackgroundSyncManager::GetRegistrations(
    int64_t sw_registration_id,
    StatusAndRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
            std::vector<std::unique_ptr<BackgroundSyncRegistration>>()));
    return;
  }

  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::GetRegistrationsImpl,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     op_scheduler_.WrapCallbackToRunNext(std::move(callback))));
}

void BackgroundSyncManager::OnRegistrationDeleted(int64_t sw_registration_id,
                                                  const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Operations already in the queue will either fail when they write to storage
  // or return stale results based on registrations loaded in memory. This is
  // inconsequential since the service worker is gone.
  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::OnRegistrationDeletedImpl,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     MakeEmptyCompletion()));
}

void BackgroundSyncManager::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Operations already in the queue will either fail when they write to storage
  // or return stale results based on registrations loaded in memory. This is
  // inconsequential since the service workers are gone.
  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::OnStorageWipedImpl,
                     weak_ptr_factory_.GetWeakPtr(), MakeEmptyCompletion()));
}

void BackgroundSyncManager::SetMaxSyncAttemptsForTesting(int max_attempts) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  op_scheduler_.ScheduleOperation(base::BindOnce(
      &BackgroundSyncManager::SetMaxSyncAttemptsImpl,
      weak_ptr_factory_.GetWeakPtr(), max_attempts, MakeEmptyCompletion()));
}

void BackgroundSyncManager::EmulateDispatchSyncEvent(
    const std::string& tag,
    scoped_refptr<ServiceWorkerVersion> active_version,
    bool last_chance,
    ServiceWorkerVersion::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blink::ServiceWorkerStatusCode code = CanEmulateSyncEvent(active_version);
  if (code != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(code);
    return;
  }
  DispatchSyncEvent(tag, std::move(active_version), last_chance,
                    std::move(callback));
}

void BackgroundSyncManager::EmulateServiceWorkerOffline(
    int64_t service_worker_id,
    bool is_offline) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Multiple DevTools sessions may want to set the same SW offline, which
  // is supposed to disable the background sync. For consistency with the
  // network stack, SW remains offline until all DevTools sessions disable
  // the offline mode.
  emulated_offline_sw_[service_worker_id] += is_offline ? 1 : -1;
  if (emulated_offline_sw_[service_worker_id] > 0)
    return;
  emulated_offline_sw_.erase(service_worker_id);
  FireReadyEvents();
}

BackgroundSyncManager::BackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : op_scheduler_(CacheStorageSchedulerClient::CLIENT_BACKGROUND_SYNC),
      service_worker_context_(service_worker_context),
      parameters_(new BackgroundSyncParameters()),
      disabled_(false),
      num_firing_registrations_(0),
      clock_(base::DefaultClock::GetInstance()),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->AddObserver(this);

#if defined(OS_ANDROID)
  network_observer_.reset(new BackgroundSyncNetworkObserverAndroid(
      base::BindRepeating(&BackgroundSyncManager::OnNetworkChanged,
                          weak_ptr_factory_.GetWeakPtr())));
#else
  network_observer_.reset(new BackgroundSyncNetworkObserver(
      base::BindRepeating(&BackgroundSyncManager::OnNetworkChanged,
                          weak_ptr_factory_.GetWeakPtr())));
#endif
}

void BackgroundSyncManager::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!op_scheduler_.ScheduledOperations());
  DCHECK(!disabled_);

  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::InitImpl,
                     weak_ptr_factory_.GetWeakPtr(), MakeEmptyCompletion()));
}

void BackgroundSyncManager::InitImpl(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&GetControllerParameters, service_worker_context_,
                     std::make_unique<BackgroundSyncParameters>(*parameters_)),
      base::BindOnce(&BackgroundSyncManager::InitDidGetControllerParameters,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncManager::InitDidGetControllerParameters(
    base::OnceClosure callback,
    std::unique_ptr<BackgroundSyncParameters> updated_parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  parameters_ = std::move(updated_parameters);
  if (parameters_->disable) {
    disabled_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  GetDataFromBackend(
      kBackgroundSyncUserDataKey,
      base::BindOnce(&BackgroundSyncManager::InitDidGetDataFromBackend,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncManager::InitDidGetDataFromBackend(
    base::OnceClosure callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != blink::ServiceWorkerStatusCode::kOk &&
      status != blink::ServiceWorkerStatusCode::kErrorNotFound) {
    LOG(ERROR) << "BackgroundSync failed to init due to backend failure.";
    DisableAndClearManager(std::move(callback));
    return;
  }

  bool corruption_detected = false;
  for (const std::pair<int64_t, std::string>& data : user_data) {
    BackgroundSyncRegistrationsProto registrations_proto;
    if (registrations_proto.ParseFromString(data.second)) {
      BackgroundSyncRegistrations* registrations =
          &active_registrations_[data.first];
      registrations->next_id = registrations_proto.next_registration_id();
      registrations->origin = GURL(registrations_proto.origin());

      for (int i = 0, max = registrations_proto.registration_size(); i < max;
           ++i) {
        const BackgroundSyncRegistrationProto& registration_proto =
            registrations_proto.registration(i);

        if (registration_proto.id() >= registrations->next_id) {
          corruption_detected = true;
          break;
        }

        BackgroundSyncRegistration* registration =
            &registrations->registration_map[registration_proto.tag()];

        BackgroundSyncRegistrationOptions* options = registration->options();
        options->tag = registration_proto.tag();
        options->network_state = registration_proto.network_state();

        registration->set_id(registration_proto.id());
        registration->set_num_attempts(registration_proto.num_attempts());
        registration->set_delay_until(
            base::Time::FromInternalValue(registration_proto.delay_until()));
      }
    }

    if (corruption_detected)
      break;
  }

  if (corruption_detected) {
    LOG(ERROR) << "Corruption detected in background sync backend";
    DisableAndClearManager(std::move(callback));
    return;
  }

  FireReadyEvents();

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void BackgroundSyncManager::RegisterCheckIfHasMainFrame(
    int64_t sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    StatusAndRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  HasMainFrameProviderHost(
      sw_registration->scope().GetOrigin(),
      base::BindOnce(&BackgroundSyncManager::RegisterDidCheckIfMainFrame,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     options, std::move(callback)));
}

void BackgroundSyncManager::RegisterDidCheckIfMainFrame(
    int64_t sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    StatusAndRegistrationCallback callback,
    bool has_main_frame_client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!has_main_frame_client) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NOT_ALLOWED,
                              std::move(callback));
    return;
  }
  RegisterImpl(sw_registration_id, options, std::move(callback));
}

void BackgroundSyncManager::RegisterImpl(
    int64_t sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    StatusAndRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              std::move(callback));
    return;
  }

  if (options.tag.length() > kMaxTagLength) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NOT_ALLOWED,
                              std::move(callback));
    return;
  }

  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&GetBackgroundSyncPermissionOnUIThread,
                     service_worker_context_,
                     sw_registration->scope().GetOrigin()),
      base::BindOnce(&BackgroundSyncManager::RegisterDidAskForPermission,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     options, std::move(callback)));
}

void BackgroundSyncManager::RegisterDidAskForPermission(
    int64_t sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    StatusAndRegistrationCallback callback,
    blink::mojom::PermissionStatus permission_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (permission_status == blink::mojom::PermissionStatus::DENIED) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_PERMISSION_DENIED,
                              std::move(callback));
    return;
  }
  DCHECK(permission_status == blink::mojom::PermissionStatus::GRANTED);

  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    // The service worker was shut down in the interim.
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&NotifyBackgroundSyncRegisteredOnUIThread,
                     service_worker_context_,
                     sw_registration->scope().GetOrigin()));

  BackgroundSyncRegistration* existing_registration =
      LookupActiveRegistration(sw_registration_id, options.tag);
  if (existing_registration) {
    DCHECK(existing_registration->options()->Equals(options));

    BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
        AreOptionConditionsMet(options)
            ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
            : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;
    BackgroundSyncMetrics::CountRegisterSuccess(
        registration_could_fire,
        BackgroundSyncMetrics::REGISTRATION_IS_DUPLICATE);

    if (existing_registration->IsFiring()) {
      existing_registration->set_sync_state(
          blink::mojom::BackgroundSyncState::REREGISTERED_WHILE_FIRING);
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), BACKGROUND_SYNC_STATUS_OK,
                       std::make_unique<BackgroundSyncRegistration>(
                           *existing_registration)));
    return;
  }

  BackgroundSyncRegistration new_registration;

  *new_registration.options() = options;

  BackgroundSyncRegistrations* registrations =
      &active_registrations_[sw_registration_id];
  new_registration.set_id(registrations->next_id++);

  AddActiveRegistration(sw_registration_id,
                        sw_registration->scope().GetOrigin(), new_registration);

  StoreRegistrations(
      sw_registration_id,
      base::BindOnce(&BackgroundSyncManager::RegisterDidStore,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     new_registration, std::move(callback)));
}

void BackgroundSyncManager::DisableAndClearManager(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != blink::ServiceWorkerStatusCode::kOk || user_data.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
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
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The status doesn't matter at this point, there is nothing else to be done.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                std::move(barrier_closure));
}

BackgroundSyncRegistration* BackgroundSyncManager::LookupActiveRegistration(
    int64_t sw_registration_id,
    const std::string& tag) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = active_registrations_.find(sw_registration_id);
  if (it == active_registrations_.end())
    return nullptr;

  BackgroundSyncRegistrations& registrations = it->second;
  DCHECK_LE(BackgroundSyncRegistration::kInitialId, registrations.next_id);
  DCHECK(!registrations.origin.is_empty());

  auto key_and_registration_iter = registrations.registration_map.find(tag);
  if (key_and_registration_iter == registrations.registration_map.end())
    return nullptr;

  return &key_and_registration_iter->second;
}

void BackgroundSyncManager::StoreRegistrations(
    int64_t sw_registration_id,
    ServiceWorkerStorage::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Serialize the data.
  const BackgroundSyncRegistrations& registrations =
      active_registrations_[sw_registration_id];
  BackgroundSyncRegistrationsProto registrations_proto;
  registrations_proto.set_next_registration_id(registrations.next_id);
  registrations_proto.set_origin(registrations.origin.spec());

  for (const auto& key_and_registration : registrations.registration_map) {
    const BackgroundSyncRegistration& registration =
        key_and_registration.second;
    BackgroundSyncRegistrationProto* registration_proto =
        registrations_proto.add_registration();
    registration_proto->set_id(registration.id());
    registration_proto->set_tag(registration.options()->tag);
    registration_proto->set_network_state(
        registration.options()->network_state);
    registration_proto->set_num_attempts(registration.num_attempts());
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
    const BackgroundSyncRegistration& new_registration,
    StatusAndRegistrationCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // The service worker registration is gone.
    active_registrations_.erase(sw_registration_id);
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              std::move(callback));
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    LOG(ERROR) << "BackgroundSync failed to store registration due to backend "
                  "failure.";
    BackgroundSyncMetrics::CountRegisterFailure(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    DisableAndClearManager(base::BindOnce(
        std::move(callback), BACKGROUND_SYNC_STATUS_STORAGE_ERROR, nullptr));
    return;
  }

  BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
      AreOptionConditionsMet(*new_registration.options())
          ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
          : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;
  BackgroundSyncMetrics::CountRegisterSuccess(
      registration_could_fire,
      BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE);

  FireReadyEvents();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), BACKGROUND_SYNC_STATUS_OK,
                                std::make_unique<BackgroundSyncRegistration>(
                                    new_registration)));
}

void BackgroundSyncManager::RemoveActiveRegistration(int64_t sw_registration_id,
                                                     const std::string& tag) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(LookupActiveRegistration(sw_registration_id, tag));

  BackgroundSyncRegistrations* registrations =
      &active_registrations_[sw_registration_id];

  registrations->registration_map.erase(tag);
}

void BackgroundSyncManager::AddActiveRegistration(
    int64_t sw_registration_id,
    const GURL& origin,
    const BackgroundSyncRegistration& sync_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sync_registration.IsValid());

  BackgroundSyncRegistrations* registrations =
      &active_registrations_[sw_registration_id];
  registrations->origin = origin;

  registrations->registration_map[sync_registration.options()->tag] =
      sync_registration;
}

void BackgroundSyncManager::StoreDataInBackend(
    int64_t sw_registration_id,
    const GURL& origin,
    const std::string& backend_key,
    const std::string& data,
    ServiceWorkerStorage::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->StoreRegistrationUserData(
      sw_registration_id, origin, {{backend_key, data}}, std::move(callback));
}

void BackgroundSyncManager::GetDataFromBackend(
    const std::string& backend_key,
    ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetUserDataForAllRegistrations(backend_key,
                                                          std::move(callback));
}

void BackgroundSyncManager::DispatchSyncEvent(
    const std::string& tag,
    scoped_refptr<ServiceWorkerVersion> active_version,
    bool last_chance,
    ServiceWorkerVersion::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(active_version);

  if (active_version->running_status() != EmbeddedWorkerStatus::RUNNING) {
    active_version->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::SYNC,
        base::BindOnce(&DidStartWorkerForSyncEvent,
                       base::BindOnce(&BackgroundSyncManager::DispatchSyncEvent,
                                      weak_ptr_factory_.GetWeakPtr(), tag,
                                      active_version, last_chance),
                       std::move(callback)));
    return;
  }

  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));

  int request_id = active_version->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC, repeating_callback,
      parameters_->max_sync_event_duration,
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);

  active_version->endpoint()->DispatchSyncEvent(
      tag, last_chance, parameters_->max_sync_event_duration,
      base::BindOnce(&OnSyncEventFinished, active_version, request_id,
                     std::move(repeating_callback)));
}

void BackgroundSyncManager::ScheduleDelayedTask(base::OnceClosure callback,
                                                base::TimeDelta delay) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, std::move(callback), delay);
}

void BackgroundSyncManager::HasMainFrameProviderHost(const GURL& origin,
                                                     BoolCallback callback) {
  service_worker_context_->HasMainFrameProviderHost(origin,
                                                    std::move(callback));
}

void BackgroundSyncManager::GetRegistrationsImpl(
    int64_t sw_registration_id,
    StatusAndRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<std::unique_ptr<BackgroundSyncRegistration>> out_registrations;

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                                  std::move(out_registrations)));
    return;
  }

  auto it = active_registrations_.find(sw_registration_id);

  if (it != active_registrations_.end()) {
    const BackgroundSyncRegistrations& registrations = it->second;
    for (const auto& tag_and_registration : registrations.registration_map) {
      const BackgroundSyncRegistration& registration =
          tag_and_registration.second;
      out_registrations.push_back(
          std::make_unique<BackgroundSyncRegistration>(registration));
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), BACKGROUND_SYNC_STATUS_OK,
                                std::move(out_registrations)));
}

bool BackgroundSyncManager::AreOptionConditionsMet(
    const BackgroundSyncRegistrationOptions& options) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return network_observer_->NetworkSufficient(options.network_state);
}

bool BackgroundSyncManager::IsRegistrationReadyToFire(
    const BackgroundSyncRegistration& registration,
    int64_t service_worker_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (registration.sync_state() != blink::mojom::BackgroundSyncState::PENDING)
    return false;

  if (clock_->Now() < registration.delay_until())
    return false;

  if (base::ContainsKey(emulated_offline_sw_, service_worker_id))
    return false;

  return AreOptionConditionsMet(*registration.options());
}

void BackgroundSyncManager::RunInBackgroundIfNecessary() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::TimeDelta soonest_wakeup_delta = base::TimeDelta::Max();

  for (const auto& sw_id_and_registrations : active_registrations_) {
    for (const auto& key_and_registration :
         sw_id_and_registrations.second.registration_map) {
      const BackgroundSyncRegistration& registration =
          key_and_registration.second;
      if (registration.sync_state() ==
          blink::mojom::BackgroundSyncState::PENDING) {
        if (clock_->Now() >= registration.delay_until()) {
          soonest_wakeup_delta = base::TimeDelta();
        } else {
          base::TimeDelta delay_delta =
              registration.delay_until() - clock_->Now();
          if (delay_delta < soonest_wakeup_delta)
            soonest_wakeup_delta = delay_delta;
        }
      }
    }
  }

  // If the browser is closed while firing events, the browser needs a task to
  // wake it back up and try again.
  if (num_firing_registrations_ > 0 &&
      soonest_wakeup_delta > parameters_->min_sync_recovery_time) {
    soonest_wakeup_delta = parameters_->min_sync_recovery_time;
  }

  // Try firing again after the wakeup delta.
  if (!soonest_wakeup_delta.is_max() && !soonest_wakeup_delta.is_zero()) {
    delayed_sync_task_.Reset(base::Bind(&BackgroundSyncManager::FireReadyEvents,
                                        weak_ptr_factory_.GetWeakPtr()));
    ScheduleDelayedTask(delayed_sync_task_.callback(), soonest_wakeup_delta);
  }

  // In case the browser closes (or to prevent it from closing), call
  // RunInBackground to either wake up the browser at the wakeup delta or to
  // keep the browser running.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          RunInBackgroundOnUIThread, service_worker_context_,
          !soonest_wakeup_delta.is_max() /* should run in background */,
          soonest_wakeup_delta.InMilliseconds()));
}

void BackgroundSyncManager::FireReadyEvents() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_)
    return;

  op_scheduler_.ScheduleOperation(
      base::BindOnce(&BackgroundSyncManager::FireReadyEventsImpl,
                     weak_ptr_factory_.GetWeakPtr(), MakeEmptyCompletion()));
}

void BackgroundSyncManager::FireReadyEventsImpl(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  // Find the registrations that are ready to run.
  std::vector<std::pair<int64_t, std::string>> sw_id_and_tags_to_fire;

  for (auto& sw_id_and_registrations : active_registrations_) {
    const int64_t service_worker_id = sw_id_and_registrations.first;
    for (auto& key_and_registration :
         sw_id_and_registrations.second.registration_map) {
      BackgroundSyncRegistration* registration = &key_and_registration.second;

      if (IsRegistrationReadyToFire(*registration, service_worker_id)) {
        sw_id_and_tags_to_fire.push_back(
            std::make_pair(service_worker_id, key_and_registration.first));
        // The state change is not saved to persistent storage because
        // if the sync event is killed mid-sync then it should return to
        // SYNC_STATE_PENDING.
        registration->set_sync_state(blink::mojom::BackgroundSyncState::FIRING);
      }
    }
  }

  if (sw_id_and_tags_to_fire.empty()) {
    RunInBackgroundIfNecessary();
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();

  // Fire the sync event of the ready registrations and run |callback| once
  // they're all done.
  base::RepeatingClosure events_fired_barrier_closure = base::BarrierClosure(
      sw_id_and_tags_to_fire.size(),
      base::BindOnce(&BackgroundSyncManager::FireReadyEventsAllEventsFiring,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  // Record the total time taken after all events have run to completion.
  base::RepeatingClosure events_completed_barrier_closure =
      base::BarrierClosure(sw_id_and_tags_to_fire.size(),
                           base::BindOnce(&OnAllSyncEventsCompleted, start_time,
                                          sw_id_and_tags_to_fire.size()));

  for (const auto& sw_id_and_tag : sw_id_and_tags_to_fire) {
    int64_t service_worker_id = sw_id_and_tag.first;
    const BackgroundSyncRegistration* registration =
        LookupActiveRegistration(service_worker_id, sw_id_and_tag.second);
    DCHECK(registration);

    service_worker_context_->FindReadyRegistrationForId(
        service_worker_id, active_registrations_[service_worker_id].origin,
        base::BindOnce(
            &BackgroundSyncManager::FireReadyEventsDidFindRegistration,
            weak_ptr_factory_.GetWeakPtr(), sw_id_and_tag.second,
            registration->id(), events_fired_barrier_closure,
            events_completed_barrier_closure));
  }
}

void BackgroundSyncManager::FireReadyEventsDidFindRegistration(
    const std::string& tag,
    BackgroundSyncRegistration::RegistrationId registration_id,
    base::OnceClosure event_fired_callback,
    base::OnceClosure event_completed_callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(event_fired_callback));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(event_completed_callback));
    return;
  }

  BackgroundSyncRegistration* registration =
      LookupActiveRegistration(service_worker_registration->id(), tag);
  DCHECK(registration);

  num_firing_registrations_ += 1;

  const bool last_chance =
      registration->num_attempts() == parameters_->max_sync_attempts - 1;

  HasMainFrameProviderHost(
      service_worker_registration->scope().GetOrigin(),
      base::BindOnce(&BackgroundSyncMetrics::RecordEventStarted));

  DispatchSyncEvent(registration->options()->tag,
                    service_worker_registration->active_version(), last_chance,
                    base::BindOnce(&BackgroundSyncManager::EventComplete,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   service_worker_registration,
                                   service_worker_registration->id(), tag,
                                   std::move(event_completed_callback)));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, std::move(event_fired_callback));
}

void BackgroundSyncManager::FireReadyEventsAllEventsFiring(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunInBackgroundIfNecessary();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

// |service_worker_registration| is just to keep the registration alive
// while the event is firing.
void BackgroundSyncManager::EventComplete(
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration,
    int64_t service_worker_id,
    const std::string& tag,
    base::OnceClosure callback,
    blink::ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  op_scheduler_.ScheduleOperation(base::BindOnce(
      &BackgroundSyncManager::EventCompleteImpl, weak_ptr_factory_.GetWeakPtr(),
      service_worker_id, tag, status_code,
      op_scheduler_.WrapCallbackToRunNext(std::move(callback))));
}

void BackgroundSyncManager::EventCompleteImpl(
    int64_t service_worker_id,
    const std::string& tag,
    blink::ServiceWorkerStatusCode status_code,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  num_firing_registrations_ -= 1;

  BackgroundSyncRegistration* registration =
      LookupActiveRegistration(service_worker_id, tag);
  if (!registration) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  DCHECK_NE(blink::mojom::BackgroundSyncState::PENDING,
            registration->sync_state());

  registration->set_num_attempts(registration->num_attempts() + 1);

  // The event ran to completion, we should count it, no matter what happens
  // from here.
  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(service_worker_id);
  if (sw_registration) {
    HasMainFrameProviderHost(
        sw_registration->scope().GetOrigin(),
        base::BindOnce(&BackgroundSyncMetrics::RecordEventResult,
                       status_code == blink::ServiceWorkerStatusCode::kOk));
  }

  bool registration_completed = true;
  bool can_retry =
      registration->num_attempts() < parameters_->max_sync_attempts;

  if (registration->sync_state() ==
      blink::mojom::BackgroundSyncState::REREGISTERED_WHILE_FIRING) {
    registration->set_sync_state(blink::mojom::BackgroundSyncState::PENDING);
    registration->set_num_attempts(0);
    registration_completed = false;
  } else if (status_code != blink::ServiceWorkerStatusCode::kOk &&
             can_retry) {  // Sync failed but can retry
    registration->set_sync_state(blink::mojom::BackgroundSyncState::PENDING);
    registration->set_delay_until(clock_->Now() +
                                  parameters_->initial_retry_delay *
                                      pow(parameters_->retry_delay_factor,
                                          registration->num_attempts() - 1));
    registration_completed = false;
  }

  if (registration_completed) {
    const std::string& registration_tag = registration->options()->tag;
    BackgroundSyncRegistration* active_registration =
        LookupActiveRegistration(service_worker_id, registration_tag);
    if (active_registration &&
        active_registration->id() == registration->id()) {
      RemoveActiveRegistration(service_worker_id, registration_tag);
    }
  }

  StoreRegistrations(
      service_worker_id,
      base::BindOnce(&BackgroundSyncManager::EventCompleteDidStore,
                     weak_ptr_factory_.GetWeakPtr(), service_worker_id,
                     std::move(callback)));
}

void BackgroundSyncManager::EventCompleteDidStore(
    int64_t service_worker_id,
    base::OnceClosure callback,
    blink::ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status_code == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // The registration is gone.
    active_registrations_.erase(service_worker_id);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  if (status_code != blink::ServiceWorkerStatusCode::kOk) {
    LOG(ERROR) << "BackgroundSync failed to store registration due to backend "
                  "failure.";
    DisableAndClearManager(std::move(callback));
    return;
  }

  // Fire any ready events and call RunInBackground if anything is waiting.
  FireReadyEvents();

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

// static
void BackgroundSyncManager::OnAllSyncEventsCompleted(
    const base::TimeTicks& start_time,
    int number_of_batched_sync_events) {
  // Record the combined time taken by all sync events.
  BackgroundSyncMetrics::RecordBatchSyncEventComplete(
      base::TimeTicks::Now() - start_time, number_of_batched_sync_events);
}

void BackgroundSyncManager::OnRegistrationDeletedImpl(
    int64_t sw_registration_id,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The backend (ServiceWorkerStorage) will delete the data, so just delete the
  // memory representation here.
  active_registrations_.erase(sw_registration_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void BackgroundSyncManager::OnStorageWipedImpl(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  active_registrations_.clear();
  disabled_ = false;
  InitImpl(std::move(callback));
}

void BackgroundSyncManager::OnNetworkChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FireReadyEvents();
}

void BackgroundSyncManager::SetMaxSyncAttemptsImpl(int max_attempts,
                                                   base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  parameters_->max_sync_attempts = max_attempts;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

base::OnceClosure BackgroundSyncManager::MakeEmptyCompletion() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return op_scheduler_.WrapCallbackToRunNext(base::DoNothing::Once());
}

blink::ServiceWorkerStatusCode BackgroundSyncManager::CanEmulateSyncEvent(
    scoped_refptr<ServiceWorkerVersion> active_version) {
  if (!active_version)
    return blink::ServiceWorkerStatusCode::kErrorFailed;
  if (!network_observer_->NetworkSufficient(NETWORK_STATE_ONLINE))
    return blink::ServiceWorkerStatusCode::kErrorNetwork;
  int64_t registration_id = active_version->registration_id();
  if (base::ContainsKey(emulated_offline_sw_, registration_id))
    return blink::ServiceWorkerStatusCode::kErrorNetwork;
  return blink::ServiceWorkerStatusCode::kOk;
}

}  // namespace content
