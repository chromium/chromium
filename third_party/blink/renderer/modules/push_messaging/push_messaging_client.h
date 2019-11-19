// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_callbacks.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace mojom {
enum class PushRegistrationStatus;
}  // namespace mojom

class KURL;
class PushSubscriptionOptions;
class ServiceWorkerRegistration;

class PushMessagingClient final : public GarbageCollected<PushMessagingClient>,
                                  public Supplement<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(PushMessagingClient);

 public:
  static const char kSupplementName[];

  explicit PushMessagingClient(LocalFrame& frame);
  ~PushMessagingClient() = default;

  static PushMessagingClient* From(LocalFrame* frame);

  void Subscribe(ServiceWorkerRegistration* service_worker_registration,
                 PushSubscriptionOptions* options,
                 bool user_gesture,
                 std::unique_ptr<PushSubscriptionCallbacks> callbacks);

 private:
  // Returns an initialized PushMessaging service. A connection will be
  // established after the first call to this method.
  mojom::blink::PushMessaging* GetPushMessagingRemote();

  void DidGetManifest(ServiceWorkerRegistration* service_worker_registration,
                      mojom::blink::PushSubscriptionOptionsPtr options,
                      bool user_gesture,
                      std::unique_ptr<PushSubscriptionCallbacks> callbacks,
                      const KURL& manifest_url,
                      mojom::blink::ManifestPtr manifest);

  void DoSubscribe(ServiceWorkerRegistration* service_worker_registration,
                   mojom::blink::PushSubscriptionOptionsPtr options,
                   bool user_gesture,
                   std::unique_ptr<PushSubscriptionCallbacks> callbacks);

  void DidSubscribe(ServiceWorkerRegistration* service_worker_registration,
                    std::unique_ptr<PushSubscriptionCallbacks> callbacks,
                    mojom::blink::PushRegistrationStatus status,
                    mojom::blink::PushSubscriptionPtr subscription);

  mojo::Remote<mojom::blink::PushMessaging> push_messaging_manager_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingClient);
};

void ProvidePushMessagingClientTo(LocalFrame& frame,
                                  PushMessagingClient* client);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_
