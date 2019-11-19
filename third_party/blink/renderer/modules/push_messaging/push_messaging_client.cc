// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_messaging_client.h"

#include <string>
#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging_status.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"
#include "third_party/blink/renderer/modules/push_messaging/push_error.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_type_converters.h"
#include "third_party/blink/renderer/modules/push_messaging/push_messaging_utils.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

// static
const char PushMessagingClient::kSupplementName[] = "PushMessagingClient";

PushMessagingClient::PushMessagingClient(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {
  // This class will be instantiated for every page load (rather than on push
  // messaging use), so there's nothing to be done in this constructor.
}

// static
PushMessagingClient* PushMessagingClient::From(LocalFrame* frame) {
  DCHECK(frame);
  return Supplement<LocalFrame>::From<PushMessagingClient>(frame);
}

mojom::blink::PushMessaging* PushMessagingClient::GetPushMessagingRemote() {
  if (!push_messaging_manager_) {
    GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
        push_messaging_manager_.BindNewPipeAndPassReceiver(
            GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  return push_messaging_manager_.get();
}

void PushMessagingClient::Subscribe(
    ServiceWorkerRegistration* service_worker_registration,
    PushSubscriptionOptions* options,
    bool user_gesture,
    std::unique_ptr<PushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  mojom::blink::PushSubscriptionOptionsPtr options_ptr =
      mojom::blink::PushSubscriptionOptions::From(options);

  // If a developer provided an application server key in |options|, skip
  // fetching the manifest.
  if (!options->applicationServerKey()->ByteLengthAsSizeT()) {
    ManifestManager* manifest_manager =
        ManifestManager::From(*GetSupplementable());
    manifest_manager->RequestManifest(
        WTF::Bind(&PushMessagingClient::DidGetManifest, WrapPersistent(this),
                  WrapPersistent(service_worker_registration),
                  std::move(options_ptr), user_gesture, std::move(callbacks)));
  } else {
    DoSubscribe(service_worker_registration, std::move(options_ptr),
                user_gesture, std::move(callbacks));
  }
}

void PushMessagingClient::DidGetManifest(
    ServiceWorkerRegistration* service_worker_registration,
    mojom::blink::PushSubscriptionOptionsPtr options,
    bool user_gesture,
    std::unique_ptr<PushSubscriptionCallbacks> callbacks,
    const KURL& manifest_url,
    mojom::blink::ManifestPtr manifest) {
  // Get the application_server_key from the manifest since it wasn't provided
  // by the caller.
  if (manifest == mojom::blink::Manifest::New()) {
    DidSubscribe(
        service_worker_registration, std::move(callbacks),
        mojom::blink::PushRegistrationStatus::MANIFEST_EMPTY_OR_MISSING,
        nullptr /* subscription */);
    return;
  }

  if (!manifest->gcm_sender_id.IsNull()) {
    StringUTF8Adaptor gcm_sender_id_as_utf8_string(manifest->gcm_sender_id);
    Vector<uint8_t> application_server_key;
    application_server_key.Append(gcm_sender_id_as_utf8_string.data(),
                                  gcm_sender_id_as_utf8_string.size());
    options->application_server_key = std::move(application_server_key);
  }

  DoSubscribe(service_worker_registration, std::move(options), user_gesture,
              std::move(callbacks));
}

void PushMessagingClient::DoSubscribe(
    ServiceWorkerRegistration* service_worker_registration,
    mojom::blink::PushSubscriptionOptionsPtr options,
    bool user_gesture,
    std::unique_ptr<PushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  if (options->application_server_key.IsEmpty()) {
    DidSubscribe(service_worker_registration, std::move(callbacks),
                 mojom::blink::PushRegistrationStatus::NO_SENDER_ID,
                 nullptr /* subscription */);
    return;
  }

  GetPushMessagingRemote()->Subscribe(
      service_worker_registration->RegistrationId(), std::move(options),
      user_gesture,
      WTF::Bind(&PushMessagingClient::DidSubscribe, WrapPersistent(this),
                WrapPersistent(service_worker_registration),
                std::move(callbacks)));
}

void PushMessagingClient::DidSubscribe(
    ServiceWorkerRegistration* service_worker_registration,
    std::unique_ptr<PushSubscriptionCallbacks> callbacks,
    mojom::blink::PushRegistrationStatus status,
    mojom::blink::PushSubscriptionPtr subscription) {
  DCHECK(callbacks);

  if (status ==
          mojom::blink::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE ||
      status == mojom::blink::PushRegistrationStatus::
                    SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE ||
      status == mojom::blink::PushRegistrationStatus::SUCCESS_FROM_CACHE) {
    DCHECK(subscription);

    callbacks->OnSuccess(PushSubscription::Create(std::move(subscription),
                                                  service_worker_registration));
  } else {
    callbacks->OnError(PushError::CreateException(
        PushRegistrationStatusToPushErrorType(status),
        PushRegistrationStatusToString(status)));
  }
}

// static
void ProvidePushMessagingClientTo(LocalFrame& frame,
                                  PushMessagingClient* client) {
  PushMessagingClient::ProvideTo(frame, client);
}

}  // namespace blink
