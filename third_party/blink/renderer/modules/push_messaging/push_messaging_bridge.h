// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_BRIDGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_BRIDGE_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class PushSubscriptionOptionsInit;
class ScriptPromiseResolver;
class ScriptState;

// The bridge is responsible for establishing and maintaining the Mojo
// connection to the permission service. It's keyed on an active Service Worker
// Registration.
//
// TODO(peter): Use the PushMessaging Mojo service directly from here.
class PushMessagingBridge final : public GarbageCollected<PushMessagingBridge>,
                                  public Supplement<ServiceWorkerRegistration> {
  USING_GARBAGE_COLLECTED_MIXIN(PushMessagingBridge);

 public:
  static const char kSupplementName[];

  static PushMessagingBridge* From(ServiceWorkerRegistration* registration);

  explicit PushMessagingBridge(ServiceWorkerRegistration& registration);
  virtual ~PushMessagingBridge();

  // Asynchronously determines the permission state for the current origin.
  ScriptPromise GetPermissionState(ScriptState* script_state,
                                   const PushSubscriptionOptionsInit* options);

 private:
  // Method to be invoked when the permission status has been retrieved from the
  // permission service. Will settle the given |resolver|.
  void DidGetPermissionState(ScriptPromiseResolver* resolver,
                             mojom::blink::PermissionStatus status);

  mojo::Remote<mojom::blink::PermissionService> permission_service_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingBridge);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_BRIDGE_H_
