// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/push_messaging_service_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/fuchsia/file_utils.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_desktop_utils.h"
#include "components/gcm_driver/gcm_driver_constants.h"
#include "components/push_messaging/push_messaging_constants.h"
#include "components/push_messaging/push_messaging_features.h"
#include "components/push_messaging/push_messaging_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace {

bool IsPermissionGranted(const GURL& requesting_origin) {
  // Very likely this should be a command line flag based solution.
  // TODO(crbug.com/424479300): Implement the permission control of using the
  // push-messaging api.
  return true;
}

void SubscriptionError(content::PushMessagingService::RegisterCallback callback,
                       blink::mojom::PushRegistrationStatus status) {
  std::move(callback).Run(
      /* subscription_id = */ std::string{}, /* endpoint = */ GURL{},
      /* expiration_time = */ std::nullopt,
      /* p256dh = */ std::vector<uint8_t>{},
      /* auth = */ std::vector<uint8_t>{}, status);
}

bool IsInvalidRequester(const GURL& origin,
                        int64_t service_worker_registration_id) {
  // It sounds very wrong if only one of them is invalid, so make the check more
  // aggressive and ignore unexpected requests.
  return origin.is_empty() ||
         service_worker_registration_id ==
             blink::mojom::kInvalidServiceWorkerRegistrationId;
}

// The maximum subscriptions (existing plus on-the-fly) supported by the
// web-engine. Subscription requests over the limit will trigger LIMIT_REACH
// error.
//
// Unlike a full functional Chrome, subscriptions on WebEngine should be
// limited, so shrink the kMaxRegistrations from 1M on Chrome to 1K.
constexpr int kMaxRegistrations = 1000;

}  // namespace

PushMessagingServiceImpl::PushMessagingServiceImpl(
    content::BrowserContext& parent_context,
    os_crypt_async::OSCryptAsync& os_crypt_async)
    : parent_context_(parent_context), os_crypt_async_(os_crypt_async) {}

PushMessagingServiceImpl::~PushMessagingServiceImpl() = default;

// PushMessagingService implementations.
void PushMessagingServiceImpl::SubscribeFromDocument(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    int render_process_id,
    int render_frame_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    bool user_gesture,
    RegisterCallback callback) {
  DoSubscribe(requesting_origin, service_worker_registration_id,
              std::move(options), std::move(callback));
}

void PushMessagingServiceImpl::SubscribeFromWorker(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    int render_process_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    RegisterCallback callback) {
  DoSubscribe(requesting_origin, service_worker_registration_id,
              std::move(options), std::move(callback));
}

void PushMessagingServiceImpl::GetSubscriptionInfo(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    const std::string& subscription_id,
    SubscriptionInfoCallback callback) {}

void PushMessagingServiceImpl::Unsubscribe(
    blink::mojom::PushUnregistrationReason reason,
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    UnregisterCallback callback) {
  // Same as DoSubscribe, PushMessagingManager shouldn't send this pair of
  // parameters to PushMessagingService, but let's make it safer.
  if (IsInvalidRequester(requesting_origin, service_worker_registration_id)) {
    std::move(callback).Run(
        blink::mojom::PushUnregistrationStatus::NO_SERVICE_WORKER);
    return;
  }

  std::optional<push_messaging::AppIdentifier> app_id =
      FindByServiceWorker(requesting_origin, service_worker_registration_id);
  if (!app_id) {
    // Unknown subscription, won't clear the service worker database.
    std::move(callback).Run(
        blink::mojom::PushUnregistrationStatus::SUCCESS_WAS_NOT_REGISTERED);
    return;
  }
  // The logic isn't same as //chrome counterpart, but if an AppIdentifier is
  // not recogonized, the subscription shouldn't be stored in the service
  // worker.
  ClearPushSubscriptionId(
      &parent_context_, requesting_origin, service_worker_registration_id,
      base::BindOnce(&PushMessagingServiceImpl::DidClearPushSubscriptionId,
                     weak_ptr_factory_.GetWeakPtr(), reason, *app_id,
                     std::move(callback)));
}

bool PushMessagingServiceImpl::SupportNonVisibleMessages() {
  return false;
}

void PushMessagingServiceImpl::DidDeleteServiceWorkerRegistration(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  Unsubscribe(
      blink::mojom::PushUnregistrationReason::SERVICE_WORKER_UNREGISTERED,
      origin, service_worker_registration_id, /* sender_id = */ std::string{},
      base::DoNothing());
}

void PushMessagingServiceImpl::DidDeleteServiceWorkerDatabase() {}

// GCMAppHandler implementations.
void PushMessagingServiceImpl::ShutdownHandler() {}

void PushMessagingServiceImpl::OnStoreReset() {}

void PushMessagingServiceImpl::OnMessage(const std::string& app_id,
                                         const gcm::IncomingMessage& message) {}

void PushMessagingServiceImpl::OnMessagesDeleted(const std::string& app_id) {}

void PushMessagingServiceImpl::OnSendError(
    const std::string& app_id,
    const gcm::GCMClient::SendErrorDetails& send_error_details) {}

void PushMessagingServiceImpl::OnSendAcknowledged(
    const std::string& app_id,
    const std::string& message_id) {}

// Private functions.
gcm::GCMDriver& PushMessagingServiceImpl::GetGCMDriver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!gcm_driver_) {
    // No predefined blocking_task_runner, create one dedicated for gcm_driver.
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));

    gcm_driver_ = gcm::CreateGCMDriverDesktop(
        std::make_unique<gcm::GCMClientFactory>(), /* prefs = */ nullptr,
        base::FilePath(base::kPersistedDataDirectoryPath)
            .Append(gcm_driver::kGCMStoreDirname),
        base::BindPostTask(
            base::SequencedTaskRunner::GetCurrentDefault(),
            base::BindRepeating(
                &PushMessagingServiceImpl::RequestProxyResolvingSocketFactory,
                weak_ptr_factory_.GetWeakPtr())),
        GetSharedURLLoaderFactory(),
        /* network_connection_tracker = */ nullptr, GetChannel(),
        GetProductCategoryForSubtypes(),
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::ID::UI),
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::ID::IO),
        blocking_task_runner, &os_crypt_async_);
  }
  CHECK(gcm_driver_);
  return *gcm_driver_.get();
}

instance_id::InstanceIDDriver& PushMessagingServiceImpl::GetInstanceIDDriver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!instance_id_driver_) {
    instance_id_driver_ =
        std::make_unique<instance_id::InstanceIDDriver>(&GetGCMDriver());
  }
  CHECK(instance_id_driver_);
  return *instance_id_driver_;
}

void PushMessagingServiceImpl::RequestProxyResolvingSocketFactory(
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  parent_context_.GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->CreateProxyResolvingSocketFactory(std::move(receiver));
}

scoped_refptr<network::SharedURLLoaderFactory>
PushMessagingServiceImpl::GetSharedURLLoaderFactory() const {
  return parent_context_.GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

version_info::Channel PushMessagingServiceImpl::GetChannel() const {
  // Only stable version of WebEngine is released to the users.
  return version_info::Channel::STABLE;
}

std::string PushMessagingServiceImpl::GetProductCategoryForSubtypes() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return "com.chrome.fuchsia";
#else
  return "org.chromium.fuchsia";
#endif
}

void PushMessagingServiceImpl::DoSubscribe(
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    RegisterCallback callback) {
  // Unlike the full Chrome experience with permission controls and notification
  // system, web engine would use a different way of managing the permissions
  // directly from the |requesting_origin| and the subscriptions from document
  // and service worker are treated the same.
  if (!IsPermissionGranted(requesting_origin)) {
    SubscriptionError(std::move(callback),
                      blink::mojom::PushRegistrationStatus::PERMISSION_DENIED);
    return;
  }

  if (pending_subscriptions_ + app_ids_.size() > kMaxRegistrations) {
    SubscriptionError(std::move(callback),
                      blink::mojom::PushRegistrationStatus::LIMIT_REACHED);
    return;
  }

  // Very likely the PushMessagingManager shouldn't call PushMessagingService
  // with the invalid origin and service_worker registration id, but let's just
  // make it safer to avoid storing invalid data at all.
  if (IsInvalidRequester(requesting_origin, service_worker_registration_id)) {
    SubscriptionError(std::move(callback),
                      blink::mojom::PushRegistrationStatus::NO_SERVICE_WORKER);
    return;
  }

  // Note, the DoSubscribe call will override the existing subscription of the
  // combination of |requesting_origin| and |service_worker_registration_id| if
  // any.

  std::string application_server_key(options->application_server_key.begin(),
                                     options->application_server_key.end());

  push_messaging::AppIdentifier app_identifier =
      FindByServiceWorker(requesting_origin, service_worker_registration_id)
          .value_or(push_messaging::AppIdentifier::Generate(
              requesting_origin, service_worker_registration_id));

  // Set time to live for GCM registration
  base::TimeDelta ttl = base::TimeDelta();

  if (base::FeatureList::IsEnabled(
          features::kPushSubscriptionWithExpirationTime)) {
    ttl = kPushSubscriptionExpirationPeriodTimeDelta;
    app_identifier.set_expiration_time(base::Time::Now() + ttl);
    CHECK(app_identifier.expiration_time());
  }

  pending_subscriptions_++;
  if (pending_subscriptions_ == 1 && app_ids_.empty()) {
    // Initial subscription, register the AppHandler.
    GetGCMDriver().AddAppHandler(push_messaging::kAppIdentifierPrefix, this);
  }

  GetInstanceIDDriver()
      .GetInstanceID(app_identifier.app_id())
      ->GetToken(push_messaging::NormalizeSenderInfo(application_server_key),
                 instance_id::kGCMScope, ttl, {} /* flags */,
                 base::BindOnce(&PushMessagingServiceImpl::DidSubscribe,
                                weak_ptr_factory_.GetWeakPtr(), app_identifier,
                                application_server_key, std::move(callback)));
}

void PushMessagingServiceImpl::DidSubscribe(
    const push_messaging::AppIdentifier& app_identifier,
    const std::string& sender_id,
    RegisterCallback callback,
    const std::string& subscription_id,
    instance_id::InstanceID::Result result) {
  blink::mojom::PushRegistrationStatus status =
      blink::mojom::PushRegistrationStatus::SERVICE_ERROR;

  switch (result) {
    case instance_id::InstanceID::SUCCESS: {
      // Make sure that this subscription has associated encryption keys prior
      // to returning it to the developer - they'll need this information in
      // order to send payloads to the user.
      if (push_messaging::AppIdentifier::UseInstanceID(
              app_identifier.app_id())) {
        auto encryption_info_callback = base::BindOnce(
            &PushMessagingServiceImpl::DidSubscribeWithEncryptionInfo,
            weak_ptr_factory_.GetWeakPtr(), app_identifier, std::move(callback),
            subscription_id,
            push_messaging::CreateEndpoint(GetChannel(), subscription_id));
        GetInstanceIDDriver()
            .GetInstanceID(app_identifier.app_id())
            ->GetEncryptionInfo(push_messaging::NormalizeSenderInfo(sender_id),
                                std::move(encryption_info_callback));
        return;
      }
      // Do not support the legacy non-InstanceID SenderIDs.
      status = blink::mojom::PushRegistrationStatus::UNSUPPORTED_GCM_SENDER_ID;
      break;
    }
    case instance_id::InstanceID::INVALID_PARAMETER:
    case instance_id::InstanceID::DISABLED:
    case instance_id::InstanceID::ASYNC_OPERATION_PENDING:
    case instance_id::InstanceID::SERVER_ERROR:
    case instance_id::InstanceID::UNKNOWN_ERROR:
      DLOG(ERROR) << "Push messaging subscription failed; InstanceID::Result = "
                  << result;
      status = blink::mojom::PushRegistrationStatus::SERVICE_ERROR;
      break;
    case instance_id::InstanceID::NETWORK_ERROR:
      status = blink::mojom::PushRegistrationStatus::NETWORK_ERROR;
      break;
  }

  pending_subscriptions_--;
  SubscriptionError(std::move(callback), status);
}

std::optional<push_messaging::AppIdentifier>
PushMessagingServiceImpl::FindByServiceWorker(
    const GURL& origin,
    int64_t service_worker_registration_id) const {
  for (const auto& [key, value] : app_ids_) {
    if (value.origin() == origin && value.service_worker_registration_id() ==
                                        service_worker_registration_id) {
      return value;
    }
  }
  return std::nullopt;
}

void PushMessagingServiceImpl::DidSubscribeWithEncryptionInfo(
    const push_messaging::AppIdentifier& app_identifier,
    RegisterCallback callback,
    const std::string& subscription_id,
    const GURL& endpoint,
    std::string p256dh,
    std::string auth_secret) {
  if (p256dh.empty()) {
    SubscriptionError(
        std::move(callback),
        blink::mojom::PushRegistrationStatus::PUBLIC_KEY_UNAVAILABLE);
    return;
  }

  pending_subscriptions_--;

  std::erase_if(app_ids_, [&app_identifier](const auto& item) {
    const auto& [_, value] = item;
    return value.origin() == app_identifier.origin() &&
           value.service_worker_registration_id() ==
               app_identifier.service_worker_registration_id();
  });
  app_ids_.emplace(app_identifier.app_id(), app_identifier);

  std::move(callback).Run(
      subscription_id, endpoint, app_identifier.expiration_time(),
      std::vector<uint8_t>(p256dh.begin(), p256dh.end()),
      std::vector<uint8_t>(auth_secret.begin(), auth_secret.end()),
      blink::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE);
}

void PushMessagingServiceImpl::DidClearPushSubscriptionId(
    blink::mojom::PushUnregistrationReason reason,
    const push_messaging::AppIdentifier& app_identifier,
    UnregisterCallback callback) {
  const std::string& app_id = app_identifier.app_id();
  CHECK_EQ(app_ids_.erase(app_id), 1u);
  std::move(callback).Run(
      blink::mojom::PushUnregistrationStatus::SUCCESS_UNREGISTERED);

  if (push_messaging::AppIdentifier::UseInstanceID(app_id)) {
    GetInstanceIDDriver().GetInstanceID(app_id)->DeleteID(
        base::BindOnce(&PushMessagingServiceImpl::DidDeleteID,
                       weak_ptr_factory_.GetWeakPtr(), app_id));

  } else {
    GetGCMDriver().Unregister(
        app_identifier.app_id(),
        base::BindOnce(&PushMessagingServiceImpl::DidUnsubscribe,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void PushMessagingServiceImpl::DidDeleteID(const std::string& app_id,
                                           instance_id::InstanceID::Result) {
  // RemoveInstanceID must be run asynchronously, since it calls
  // InstanceIDDriver::RemoveInstanceID which deletes the InstanceID itself.
  // Calling that immediately would cause a use-after-free in our caller.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PushMessagingServiceImpl::RemoveInstanceID,
                                weak_ptr_factory_.GetWeakPtr(), app_id));
}

void PushMessagingServiceImpl::RemoveInstanceID(const std::string& app_id) {
  GetInstanceIDDriver().RemoveInstanceID(app_id);
  DidUnsubscribe(gcm::GCMClient::Result::SUCCESS);
}

void PushMessagingServiceImpl::DidUnsubscribe(gcm::GCMClient::Result) {
  if (pending_subscriptions_ == 0 && app_ids_.empty()) {
    GetGCMDriver().RemoveAppHandler(push_messaging::kAppIdentifierPrefix);
  }
}
