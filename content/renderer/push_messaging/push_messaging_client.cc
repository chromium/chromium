// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/push_messaging/push_messaging_client.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_thread_impl.h"
#include "content/common/push_messaging.mojom.h"
#include "content/public/common/push_messaging_status.mojom.h"
#include "content/public/common/service_names.mojom.h"
#include "content/renderer/push_messaging/push_provider.h"
#include "content/renderer/render_frame_impl.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_error.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_subscription.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_subscription_options.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

namespace content {

PushMessagingClient::PushMessagingClient(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
  if (ChildThreadImpl::current()) {
    ChildThreadImpl::current()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName,
        mojo::MakeRequest(&push_messaging_manager_));
  }
}

PushMessagingClient::~PushMessagingClient() {}

void PushMessagingClient::OnDestruct() {
  delete this;
}

void PushMessagingClient::Subscribe(
    int64_t service_worker_registration_id,
    const blink::WebPushSubscriptionOptions& options,
    bool user_gesture,
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(callbacks);

  // If a developer provided an application server key in |options|, skip
  // fetching the manifest.
  if (options.application_server_key.IsEmpty()) {
    RenderFrameImpl::FromRoutingID(routing_id())
        ->GetManifestManager()
        .RequestManifest(base::BindOnce(&PushMessagingClient::DidGetManifest,
                                        base::Unretained(this),
                                        service_worker_registration_id, options,
                                        user_gesture, std::move(callbacks)));
  } else {
    PushSubscriptionOptions content_options;
    content_options.user_visible_only = options.user_visible_only;
    // Just treat the server key as a string of bytes and pass it to the push
    // service.
    content_options.sender_info = options.application_server_key.Latin1();
    DoSubscribe(service_worker_registration_id, content_options, user_gesture,
                std::move(callbacks));
  }
}

void PushMessagingClient::DidGetManifest(
    int64_t service_worker_registration_id,
    const blink::WebPushSubscriptionOptions& options,
    bool user_gesture,
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
    const GURL& manifest_url,
    const blink::Manifest& manifest) {
  // Get the sender_info from the manifest since it wasn't provided by
  // the caller.
  if (manifest.IsEmpty()) {
    DidSubscribe(std::move(callbacks),
                 mojom::PushRegistrationStatus::MANIFEST_EMPTY_OR_MISSING,
                 base::nullopt, base::nullopt, base::nullopt, base::nullopt);
    return;
  }

  PushSubscriptionOptions content_options;
  content_options.user_visible_only = options.user_visible_only;
  if (!manifest.gcm_sender_id.is_null()) {
    content_options.sender_info =
        base::UTF16ToUTF8(manifest.gcm_sender_id.string());
  }

  DoSubscribe(service_worker_registration_id, content_options, user_gesture,
              std::move(callbacks));
}

void PushMessagingClient::DoSubscribe(
    int64_t service_worker_registration_id,
    const PushSubscriptionOptions& options,
    bool user_gesture,
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) {
  if (options.sender_info.empty()) {
    DidSubscribe(std::move(callbacks),
                 mojom::PushRegistrationStatus::NO_SENDER_ID, base::nullopt,
                 base::nullopt, base::nullopt, base::nullopt);
    return;
  }

  DCHECK(push_messaging_manager_);
  push_messaging_manager_->Subscribe(
      routing_id(), service_worker_registration_id, options, user_gesture,
      // Safe to use base::Unretained because |push_messaging_manager_ |is
      // owned by |this|.
      base::BindOnce(&PushMessagingClient::DidSubscribe, base::Unretained(this),
                     std::move(callbacks)));
}

void PushMessagingClient::DidSubscribe(
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
    mojom::PushRegistrationStatus status,
    const base::Optional<GURL>& endpoint,
    const base::Optional<PushSubscriptionOptions>& options,
    const base::Optional<std::vector<uint8_t>>& p256dh,
    const base::Optional<std::vector<uint8_t>>& auth) {
  DCHECK(callbacks);

  if (status == mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE ||
      status == mojom::PushRegistrationStatus::
                    SUCCESS_NEW_SUBSCRIPTION_FROM_PUSH_SERVICE ||
      status == mojom::PushRegistrationStatus::SUCCESS_FROM_CACHE) {
    DCHECK(endpoint);
    DCHECK(options);
    DCHECK(p256dh);
    DCHECK(auth);

    callbacks->OnSuccess(std::make_unique<blink::WebPushSubscription>(
        endpoint.value(), options.value().user_visible_only,
        blink::WebString::FromLatin1(options.value().sender_info),
        p256dh.value(), auth.value()));
  } else {
    callbacks->OnError(PushRegistrationStatusToWebPushError(status));
  }
}

}  // namespace content
