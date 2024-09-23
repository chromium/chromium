// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/push_messaging/push_messaging_manager.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "content/browser/bad_message.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom.h"
#include "url/origin.h"

namespace content {

// Service Worker database keys. If a registration ID is stored, the stored
// sender ID must be the one used to register. Unfortunately, this isn't always
// true of pre-InstanceID registrations previously stored in the database, but
// fortunately it's less important for their sender ID to be accurate.
const char kPushSenderIdServiceWorkerKey[] = "push_sender_id";
const char kPushRegistrationIdServiceWorkerKey[] = "push_registration_id";

namespace {

// Chrome currently does not support the Push API in incognito.
const char kIncognitoPushUnsupportedMessage[] =
    "Chrome currently does not support the Push API in incognito mode "
    "(https://crbug.com/401439). There is deliberately no way to "
    "feature-detect this, since incognito mode needs to be undetectable by "
    "websites.";

// These UMA methods are called from the SW and/or UI threads. Racey but ok, see
// https://groups.google.com/a/chromium.org/d/msg/chromium-dev/FNzZRJtN2aw/Aw0CWAXJJ1kJ
void RecordRegistrationStatus(blink::mojom::PushRegistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.RegistrationStatus", status);
}
void RecordUnregistrationStatus(blink::mojom::PushUnregistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.UnregistrationStatus", status);
}
void RecordGetRegistrationStatus(
    blink::mojom::PushGetRegistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.GetRegistrationStatus", status);
}

const char* PushUnregistrationStatusToString(
    blink::mojom::PushUnregistrationStatus status) {
  switch (status) {
    case blink::mojom::PushUnregistrationStatus::SUCCESS_UNREGISTERED:
      return "Unregistration successful - from push service";

    case blink::mojom::PushUnregistrationStatus::SUCCESS_WAS_NOT_REGISTERED:
      return "Unregistration successful - was not registered";

    case blink::mojom::PushUnregistrationStatus::PENDING_NETWORK_ERROR:
      return "Unregistration pending - a network error occurred, but it will "
             "be retried until it succeeds";

    case blink::mojom::PushUnregistrationStatus::NO_SERVICE_WORKER:
      return "Unregistration failed - no Service Worker";

    case blink::mojom::PushUnregistrationStatus::SERVICE_NOT_AVAILABLE:
      return "Unregistration failed - push service not available";

    case blink::mojom::PushUnregistrationStatus::PENDING_SERVICE_ERROR:
      return "Unregistration pending - a push service error occurred, but it "
             "will be retried until it succeeds";

    case blink::mojom::PushUnregistrationStatus::STORAGE_ERROR:
      return "Unregistration failed - storage error";

    case blink::mojom::PushUnregistrationStatus::NETWORK_ERROR:
      return "Unregistration failed - could not connect to push server";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

// Returns application_server_key if non-empty, otherwise checks if
// stored_sender_id may be used as a fallback and if so, returns
// stored_sender_id instead.
//
// This is in order to support the legacy way of subscribing from a service
// worker (first subscribe from the document using a gcm_sender_id set in the
// manifest, and then subscribe from the service worker with no key).
//
// An empty string will be returned if application_server_key is empty and the
// fallback is not a numeric gcm sender id.
std::string FixSenderInfo(const std::string& application_server_key,
                          const std::string& stored_sender_id) {
  if (!application_server_key.empty())
    return application_server_key;
  if (base::ContainsOnlyChars(stored_sender_id, "0123456789"))
    return stored_sender_id;
  return std::string();
}

bool IsRequestFromDocument(int render_frame_id) {
  return render_frame_id != ChildProcessHost::kInvalidUniqueID;
}

}  // namespace

struct PushMessagingManager::RegisterData {
  RegisterData() = default;
  RegisterData(RegisterData&& other) = default;

  blink::StorageKey requesting_storage_key{};
  int64_t service_worker_registration_id{0};
  std::optional<std::string> existing_subscription_id;
  blink::mojom::PushSubscriptionOptionsPtr options;
  SubscribeCallback callback;

  // True if the call to register was made with a user gesture.
  bool user_gesture;
};

PushMessagingManager::PushMessagingManager(
    RenderProcessHost& render_process_host,
    int render_frame_id,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : render_process_host_(render_process_host),
      render_frame_id_(render_frame_id),
      service_worker_context_(std::move(service_worker_context)),
      is_incognito_(
          render_process_host_->GetBrowserContext()->IsOffTheRecord()),
      service_available_(!!GetService()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PushMessagingManager::~PushMessagingManager() {}

void PushMessagingManager::AddPushMessagingReceiver(
    mojo::PendingReceiver<blink::mojom::PushMessaging> receiver) {
  receivers_.Add(this, std::move(receiver));
}

// Subscribe methods, merged in order of use.
// -----------------------------------------------------------------------------

void PushMessagingManager::Subscribe(
    int64_t service_worker_registration_id,
    blink::mojom::PushSubscriptionOptionsPtr options,
    bool user_gesture,
    SubscribeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(options);

  RegisterData data;

  data.service_worker_registration_id = service_worker_registration_id;
  data.callback = std::move(callback);
  data.options = std::move(options);
  data.user_gesture = user_gesture;

  scoped_refptr<ServiceWorkerRegistration> service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          data.service_worker_registration_id);
  if (!service_worker_registration ||
      !service_worker_registration->active_version()) {
    SendSubscriptionError(
        std::move(data),
        blink::mojom::PushRegistrationStatus::NO_SERVICE_WORKER);
    return;
  }

  // The renderer should have checked and disallowed the request for fenced
  // frames and thrown an exception in blink::PushManager. Report a bad message
  // if the renderer if the renderer side check didn't happen for some reason.
  if (service_worker_registration->ancestor_frame_type() ==
      blink::mojom::AncestorFrameType::kFencedFrame) {
    bad_message::ReceivedBadMessage(render_process_host_->GetID(),
                                    bad_message::PMM_SUBSCRIBE_IN_FENCED_FRAME);
    return;
  }

  const blink::StorageKey& storage_key = service_worker_registration->key();

  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanAccessDataForOrigin(
          render_process_host_->GetID(), storage_key.origin())) {
    bad_message::ReceivedBadMessage(&*render_process_host_,
                                    bad_message::PMM_SUBSCRIBE_INVALID_ORIGIN);
    return;
  }

  data.requesting_storage_key = storage_key;

  DCHECK(!(data.options->application_server_key.empty() &&
           IsRequestFromDocument(render_frame_id_)));

  int64_t registration_id = data.service_worker_registration_id;
  service_worker_context_->GetRegistrationUserData(
      registration_id,
      {kPushRegistrationIdServiceWorkerKey, kPushSenderIdServiceWorkerKey},
      base::BindOnce(&PushMessagingManager::DidCheckForExistingRegistration,
                     weak_factory_.GetWeakPtr(), std::move(data)));
}

void PushMessagingManager::DidCheckForExistingRegistration(
    RegisterData data,
    const std::vector<std::string>& subscription_id_and_sender_id,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Validate the stored subscription against the subscription request made by
  // the developer. The authorized entity must match.
  if (service_worker_status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK_EQ(2u, subscription_id_and_sender_id.size());

    const std::string& subscription_id = subscription_id_and_sender_id[0];
    const std::string& stored_sender_id = subscription_id_and_sender_id[1];

    const std::string application_server_key_string(
        data.options->application_server_key.begin(),
        data.options->application_server_key.end());

    std::string fixed_sender_id(
        FixSenderInfo(application_server_key_string, stored_sender_id));
    if (fixed_sender_id.empty()) {
      SendSubscriptionError(std::move(data),
                            blink::mojom::PushRegistrationStatus::NO_SENDER_ID);
      return;
    }

    if (fixed_sender_id != stored_sender_id) {
      SendSubscriptionError(
          std::move(data),
          blink::mojom::PushRegistrationStatus::SENDER_ID_MISMATCH);
      return;
    }

    data.existing_subscription_id = subscription_id;
  }

  // TODO(peter): Handle failures other than
  // blink::ServiceWorkerStatusCode::kErrorNotFound by rejecting
  // the subscription algorithm instead of trying to subscribe.

  if (!data.options->application_server_key.empty()) {
    Register(std::move(data));
  } else {
    // No |application_server_key| was provided by the developer. Fall back to
    // checking whether a previous subscription did identify a sender.
    int64_t registration_id = data.service_worker_registration_id;
    service_worker_context_->GetRegistrationUserData(
        registration_id, {kPushSenderIdServiceWorkerKey},
        base::BindOnce(&PushMessagingManager::DidGetSenderIdFromStorage,
                       weak_factory_.GetWeakPtr(), std::move(data)));
  }
}

void PushMessagingManager::DidGetSenderIdFromStorage(
    RegisterData data,
    const std::vector<std::string>& stored_sender_id,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    SendSubscriptionError(std::move(data),
                          blink::mojom::PushRegistrationStatus::NO_SENDER_ID);
    return;
  }
  DCHECK_EQ(1u, stored_sender_id.size());
  // We should only be here because no sender info was supplied to subscribe().
  DCHECK(data.options->application_server_key.empty());

  const std::string application_server_key_string(
      std::string(data.options->application_server_key.begin(),
                  data.options->application_server_key.end()));
  std::string fixed_sender_id(
      FixSenderInfo(application_server_key_string, stored_sender_id[0]));
  if (fixed_sender_id.empty()) {
    SendSubscriptionError(std::move(data),
                          blink::mojom::PushRegistrationStatus::NO_SENDER_ID);
    return;
  }
  data.options->application_server_key =
      std::vector<uint8_t>(fixed_sender_id.begin(), fixed_sender_id.end());
  Register(std::move(data));
}

void PushMessagingManager::Register(PushMessagingManager::RegisterData data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService* push_service = GetService();
  if (!push_service) {
    if (!is_incognito_) {
      // This might happen if InstanceIDProfileService::IsInstanceIDEnabled
      // returns false because the Instance ID kill switch was enabled.
      SendSubscriptionError(
          std::move(data),
          blink::mojom::PushRegistrationStatus::SERVICE_NOT_AVAILABLE);
    } else {
      // Prevent websites from detecting incognito mode, by emulating what would
      // have happened if we had a PushMessagingService available.
      if (!IsRequestFromDocument(render_frame_id_) ||
          !data.options->user_visible_only) {
        // Throw a permission denied error under the same circumstances.
        SendSubscriptionError(
            std::move(data),
            blink::mojom::PushRegistrationStatus::INCOGNITO_PERMISSION_DENIED);
      } else {
        RenderFrameHostImpl* render_frame_host_impl =
            RenderFrameHostImpl::FromID(render_process_host_->GetID(),
                                        render_frame_id_);
        if (render_frame_host_impl) {
          render_frame_host_impl->AddMessageToConsole(
              blink::mojom::ConsoleMessageLevel::kError,
              kIncognitoPushUnsupportedMessage);

          // Request notifications permission (which will fail, since
          // notifications aren't supported in incognito), so the website can't
          // detect whether incognito is active.
          bool user_gesture = data.user_gesture;

          DCHECK_EQ(data.requesting_storage_key,
                    render_frame_host_impl->GetStorageKey());

          render_frame_host_impl->GetBrowserContext()
              ->GetPermissionController()
              ->RequestPermissionFromCurrentDocument(
                  render_frame_host_impl,
                  PermissionRequestDescription(
                      blink::PermissionType::NOTIFICATIONS, user_gesture),
                  base::BindOnce(
                      &PushMessagingManager::DidRequestPermissionInIncognito,
                      AsWeakPtr(), std::move(data)));
        }
      }
    }
    return;
  }

  int64_t registration_id = data.service_worker_registration_id;
  url::Origin requesting_origin = data.requesting_storage_key.origin();
  bool user_gesture = data.user_gesture;

  auto options = data.options->Clone();
  if (IsRequestFromDocument(render_frame_id_)) {
    push_service->SubscribeFromDocument(
        requesting_origin.GetURL(), registration_id,
        render_process_host_->GetID(), render_frame_id_, std::move(options),
        user_gesture,
        base::BindOnce(&PushMessagingManager::DidRegister, AsWeakPtr(),
                       std::move(data)));
  } else {
    push_service->SubscribeFromWorker(
        requesting_origin.GetURL(), registration_id,
        render_process_host_->GetID(), std::move(options),
        base::BindOnce(&PushMessagingManager::DidRegister, AsWeakPtr(),
                       std::move(data)));
  }
}

void PushMessagingManager::DidRequestPermissionInIncognito(
    RegisterData data,
    blink::mojom::PermissionStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Notification permission should always be denied in incognito.
  DCHECK_EQ(blink::mojom::PermissionStatus::DENIED, status);
  SendSubscriptionError(
      std::move(data),
      blink::mojom::PushRegistrationStatus::INCOGNITO_PERMISSION_DENIED);
}

// TODO(crbug.com/40139581): Handle expiration_time that is passed from push
// service check if |expiration_time| is valid before saving it in |data| and
// passing it back in SendSubscriptionSuccess.
void PushMessagingManager::DidRegister(
    RegisterData data,
    const std::string& push_subscription_id,
    const GURL& endpoint,
    const std::optional<base::Time>& expiration_time,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth,
    blink::mojom::PushRegistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/41275327): Handle the case where |push_subscription_id| and
  // |data.existing_subscription_id| are not the same. Right now we just
  // override the old subscription ID and encryption information.
  const bool subscription_changed =
      data.existing_subscription_id.has_value() &&
      data.existing_subscription_id.value() != push_subscription_id;

  if (status ==
      blink::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE) {
    PersistRegistration(
        std::move(data), push_subscription_id, endpoint, expiration_time,
        p256dh, auth,
        subscription_changed
            ? blink::mojom::PushRegistrationStatus::
                  SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE
            : blink::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE);
  } else {
    // TODO(crbug.com/41275327): for invalid |expiration_time| send a
    // subscription error with a new PushRegistrationStatus
    SendSubscriptionError(std::move(data), status);
  }
}

void PushMessagingManager::PersistRegistration(
    RegisterData data,
    const std::string& push_subscription_id,
    const GURL& endpoint,
    const std::optional<base::Time>& expiration_time,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth,
    blink::mojom::PushRegistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::StorageKey storage_key = data.requesting_storage_key;
  int64_t registration_id = data.service_worker_registration_id;
  std::string application_server_key(
      std::string(data.options->application_server_key.begin(),
                  data.options->application_server_key.end()));

  service_worker_context_->StoreRegistrationUserData(
      registration_id, std::move(storage_key),
      {{kPushRegistrationIdServiceWorkerKey, push_subscription_id},
       {kPushSenderIdServiceWorkerKey, application_server_key}},
      base::BindOnce(&PushMessagingManager::DidPersistRegistration,
                     weak_factory_.GetWeakPtr(), std::move(data), endpoint,
                     expiration_time, p256dh, auth, status));
}

void PushMessagingManager::DidPersistRegistration(
    RegisterData data,
    const GURL& endpoint,
    const std::optional<base::Time>& expiration_time,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth,
    blink::mojom::PushRegistrationStatus push_registration_status,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_worker_status == blink::ServiceWorkerStatusCode::kOk) {
    SendSubscriptionSuccess(std::move(data), push_registration_status, endpoint,
                            expiration_time, p256dh, auth);
  } else {
    // TODO(johnme): Unregister, so PushMessagingServiceImpl can decrease count.
    SendSubscriptionError(std::move(data),
                          blink::mojom::PushRegistrationStatus::STORAGE_ERROR);
  }
}

void PushMessagingManager::SendSubscriptionError(
    RegisterData data,
    blink::mojom::PushRegistrationStatus status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(data.callback).Run(status, nullptr /* subscription */);
  RecordRegistrationStatus(status);
}

void PushMessagingManager::SendSubscriptionSuccess(
    RegisterData data,
    blink::mojom::PushRegistrationStatus status,
    const GURL& endpoint,
    const std::optional<base::Time>& expiration_time,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!service_available_) {
    // This shouldn't be possible in incognito mode, since we've already checked
    // that we have an existing registration. Hence it's ok to throw an error.
    DCHECK(!is_incognito_);
    SendSubscriptionError(
        std::move(data),
        blink::mojom::PushRegistrationStatus::SERVICE_NOT_AVAILABLE);
    return;
  }

  std::move(data.callback)
      .Run(status, blink::mojom::PushSubscription::New(
                       endpoint, expiration_time, std::move(data.options),
                       p256dh, auth));

  RecordRegistrationStatus(status);
}

// Unsubscribe methods, merged in order of use.
// -----------------------------------------------------------------------------

void PushMessagingManager::Unsubscribe(int64_t service_worker_registration_id,
                                       UnsubscribeCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  scoped_refptr<ServiceWorkerRegistration> service_worker_registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (!service_worker_registration) {
    DidUnregister(std::move(callback),
                  blink::mojom::PushUnregistrationStatus::NO_SERVICE_WORKER);
    return;
  }

  const url::Origin& origin = service_worker_registration->key().origin();

  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanAccessDataForOrigin(
          render_process_host_->GetID(), origin)) {
    bad_message::ReceivedBadMessage(
        &*render_process_host_, bad_message::PMM_UNSUBSCRIBE_INVALID_ORIGIN);
    return;
  }

  service_worker_context_->GetRegistrationUserData(
      service_worker_registration_id, {kPushSenderIdServiceWorkerKey},
      base::BindOnce(&PushMessagingManager::UnsubscribeHavingGottenSenderId,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     service_worker_registration_id, origin));
}

void PushMessagingManager::UnsubscribeHavingGottenSenderId(
    UnsubscribeCallback callback,
    int64_t service_worker_registration_id,
    const url::Origin& requesting_origin,
    const std::vector<std::string>& sender_ids,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::string sender_id;
  if (service_worker_status == blink::ServiceWorkerStatusCode::kOk) {
    DCHECK_EQ(1u, sender_ids.size());
    sender_id = sender_ids[0];
  }

  PushMessagingService* push_service = GetService();
  if (!push_service) {
    // This shouldn't be possible in incognito mode, since we've already checked
    // that we have an existing registration. Hence it's ok to throw an error.
    DCHECK(!is_incognito_);
    DidUnregister(
        std::move(callback),
        blink::mojom::PushUnregistrationStatus::SERVICE_NOT_AVAILABLE);
    return;
  }

  push_service->Unsubscribe(
      blink::mojom::PushUnregistrationReason::JAVASCRIPT_API,
      requesting_origin.GetURL(), service_worker_registration_id, sender_id,
      base::BindOnce(&PushMessagingManager::DidUnregister, AsWeakPtr(),
                     std::move(callback)));
}

void PushMessagingManager::DidUnregister(
    UnsubscribeCallback callback,
    blink::mojom::PushUnregistrationStatus unregistration_status) {
  // Only called from SW thread, but would be safe to call from UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (unregistration_status) {
    case blink::mojom::PushUnregistrationStatus::SUCCESS_UNREGISTERED:
    case blink::mojom::PushUnregistrationStatus::PENDING_NETWORK_ERROR:
    case blink::mojom::PushUnregistrationStatus::PENDING_SERVICE_ERROR:
      std::move(callback).Run(blink::mojom::PushErrorType::NONE,
                              true /* did_unsubscribe */,
                              std::nullopt /* error_message */);
      break;
    case blink::mojom::PushUnregistrationStatus::SUCCESS_WAS_NOT_REGISTERED:
      std::move(callback).Run(blink::mojom::PushErrorType::NONE,
                              false /* did_unsubscribe */,
                              std::nullopt /* error_message */);
      break;
    case blink::mojom::PushUnregistrationStatus::NO_SERVICE_WORKER:
    case blink::mojom::PushUnregistrationStatus::SERVICE_NOT_AVAILABLE:
    case blink::mojom::PushUnregistrationStatus::STORAGE_ERROR:
      std::move(callback).Run(blink::mojom::PushErrorType::ABORT, false,
                              std::string(PushUnregistrationStatusToString(
                                  unregistration_status)) /* error_message */);
      break;
    case blink::mojom::PushUnregistrationStatus::NETWORK_ERROR:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  RecordUnregistrationStatus(unregistration_status);
}

// GetSubscription methods, merged in order of use.
// -----------------------------------------------------------------------------

void PushMessagingManager::GetSubscription(
    int64_t service_worker_registration_id,
    GetSubscriptionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_refptr<ServiceWorkerRegistration> registration =
      service_worker_context_->GetLiveRegistration(
          service_worker_registration_id);
  if (registration) {
    if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanAccessDataForOrigin(
            render_process_host_->GetID(), registration->key().origin())) {
      bad_message::ReceivedBadMessage(
          &*render_process_host_,
          bad_message::PMM_GET_SUBSCRIPTION_INVALID_ORIGIN);
      return;
    }
  }

  service_worker_context_->GetRegistrationUserData(
      service_worker_registration_id,
      {kPushRegistrationIdServiceWorkerKey, kPushSenderIdServiceWorkerKey},
      base::BindOnce(&PushMessagingManager::DidGetSubscription,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     service_worker_registration_id));
}

void PushMessagingManager::DidGetSubscription(
    GetSubscriptionCallback callback,
    int64_t service_worker_registration_id,
    const std::vector<std::string>&
        push_subscription_id_and_application_server_key,
    blink::ServiceWorkerStatusCode service_worker_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::mojom::PushGetRegistrationStatus get_status =
      blink::mojom::PushGetRegistrationStatus::STORAGE_ERROR;
  switch (service_worker_status) {
    case blink::ServiceWorkerStatusCode::kOk: {
      DCHECK_EQ(2u, push_subscription_id_and_application_server_key.size());
      const std::string& push_subscription_id =
          push_subscription_id_and_application_server_key[0];
      const std::string& application_server_key =
          push_subscription_id_and_application_server_key[1];

      if (!service_available_) {
        // Return not found in incognito mode, so websites can't detect it.
        get_status = is_incognito_ ? blink::mojom::PushGetRegistrationStatus::
                                         INCOGNITO_REGISTRATION_NOT_FOUND
                                   : blink::mojom::PushGetRegistrationStatus::
                                         SERVICE_NOT_AVAILABLE;
        break;
      }

      scoped_refptr<ServiceWorkerRegistration> registration =
          service_worker_context_->GetLiveRegistration(
              service_worker_registration_id);
      if (!registration) {
        get_status =
            blink::mojom::PushGetRegistrationStatus::NO_LIVE_SERVICE_WORKER;
        break;
      }

      const url::Origin& origin = registration->key().origin();

      GetSubscriptionInfo(
          origin, service_worker_registration_id, application_server_key,
          push_subscription_id,
          base::BindOnce(&PushMessagingManager::GetSubscriptionDidGetInfo,
                         AsWeakPtr(), std::move(callback), origin,
                         service_worker_registration_id,
                         application_server_key));

      return;
    }
    case blink::ServiceWorkerStatusCode::kErrorNotFound: {
      get_status =
          blink::mojom::PushGetRegistrationStatus::REGISTRATION_NOT_FOUND;
      break;
    }
    case blink::ServiceWorkerStatusCode::kErrorFailed: {
      get_status = blink::mojom::PushGetRegistrationStatus::STORAGE_ERROR;
      break;
    }
    case blink::ServiceWorkerStatusCode::kErrorAbort:
    case blink::ServiceWorkerStatusCode::kErrorStartWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorProcessNotFound:
    case blink::ServiceWorkerStatusCode::kErrorExists:
    case blink::ServiceWorkerStatusCode::kErrorInstallWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorActivateWorkerFailed:
    case blink::ServiceWorkerStatusCode::kErrorIpcFailed:
    case blink::ServiceWorkerStatusCode::kErrorNetwork:
    case blink::ServiceWorkerStatusCode::kErrorSecurity:
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
    case blink::ServiceWorkerStatusCode::kErrorState:
    case blink::ServiceWorkerStatusCode::kErrorTimeout:
    case blink::ServiceWorkerStatusCode::kErrorScriptEvaluateFailed:
    case blink::ServiceWorkerStatusCode::kErrorDiskCache:
    case blink::ServiceWorkerStatusCode::kErrorRedundant:
    case blink::ServiceWorkerStatusCode::kErrorDisallowed:
    case blink::ServiceWorkerStatusCode::kErrorInvalidArguments:
    case blink::ServiceWorkerStatusCode::kErrorStorageDisconnected:
    case blink::ServiceWorkerStatusCode::kErrorStorageDataCorrupted: {
      DUMP_WILL_BE_NOTREACHED()
          << "Got unexpected error code: "
          << static_cast<uint32_t>(service_worker_status) << " "
          << blink::ServiceWorkerStatusToString(service_worker_status);
      get_status = blink::mojom::PushGetRegistrationStatus::STORAGE_ERROR;
      break;
    }
  }
  std::move(callback).Run(get_status, nullptr /* subscription */);
  RecordGetRegistrationStatus(get_status);
}

void PushMessagingManager::GetSubscriptionDidGetInfo(
    GetSubscriptionCallback callback,
    const url::Origin& origin,
    int64_t service_worker_registration_id,
    const std::string& application_server_key,
    bool is_valid,
    const GURL& endpoint,
    const std::optional<base::Time>& expiration_time,
    const std::vector<uint8_t>& p256dh,
    const std::vector<uint8_t>& auth) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_valid) {
    auto options = blink::mojom::PushSubscriptionOptions::New();

    // Chrome rejects subscription requests with userVisibleOnly false, so it
    // must have been true. TODO(harkness): If Chrome starts accepting silent
    // push subscriptions with userVisibleOnly false, the bool will need to be
    // stored.
    options->user_visible_only = true;
    options->application_server_key = std::vector<uint8_t>(
        application_server_key.begin(), application_server_key.end());

    blink::mojom::PushGetRegistrationStatus status =
        blink::mojom::PushGetRegistrationStatus::SUCCESS;

    std::move(callback).Run(status, blink::mojom::PushSubscription::New(
                                        endpoint, expiration_time,
                                        std::move(options), p256dh, auth));

    RecordGetRegistrationStatus(status);
  } else {
    PushMessagingService* push_service = GetService();
    if (!push_service) {
      // Shouldn't be possible to have a stored push subscription in a profile
      // with no push service, but this case can occur when the renderer is
      // shutting down.
      std::move(callback).Run(
          blink::mojom::PushGetRegistrationStatus::RENDERER_SHUTDOWN,
          nullptr /* subscription */);
      return;
    }

    // Uh-oh! Although there was a cached subscription in the Service Worker
    // database, it did not have matching counterparts in the
    // PushMessagingAppIdentifier map and/or GCM Store. Unsubscribe to fix this
    // inconsistency.
    blink::mojom::PushGetRegistrationStatus status =
        blink::mojom::PushGetRegistrationStatus::STORAGE_CORRUPT;

    push_service->Unsubscribe(
        blink::mojom::PushUnregistrationReason::
            GET_SUBSCRIPTION_STORAGE_CORRUPT,
        origin.GetURL(), service_worker_registration_id, application_server_key,
        base::BindOnce(&PushMessagingManager::GetSubscriptionDidUnsubscribe,
                       AsWeakPtr(), std::move(callback), status));

    RecordGetRegistrationStatus(status);
  }
}

void PushMessagingManager::GetSubscriptionDidUnsubscribe(
    GetSubscriptionCallback callback,
    blink::mojom::PushGetRegistrationStatus get_status,
    blink::mojom::PushUnregistrationStatus unsubscribe_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(get_status, nullptr /* subscription */);
}

void PushMessagingManager::GetSubscriptionInfo(
    const url::Origin& origin,
    int64_t service_worker_registration_id,
    const std::string& sender_id,
    const std::string& push_subscription_id,
    PushMessagingService::SubscriptionInfoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingService* push_service = GetService();
  if (!push_service) {
    std::move(callback).Run(false /* is_valid */, GURL() /* endpoint */,
                            std::nullopt /* expiration_time */,
                            std::vector<uint8_t>() /* p256dh */,
                            std::vector<uint8_t>() /* auth */);
    return;
  }

  push_service->GetSubscriptionInfo(origin.GetURL(),
                                    service_worker_registration_id, sender_id,
                                    push_subscription_id, std::move(callback));
}

PushMessagingService* PushMessagingManager::GetService() {
  return render_process_host_->GetBrowserContext()->GetPushMessagingService();
}

}  // namespace content
