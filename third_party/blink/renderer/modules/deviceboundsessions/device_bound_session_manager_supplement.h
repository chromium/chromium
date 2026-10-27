// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICEBOUNDSESSIONS_DEVICE_BOUND_SESSION_MANAGER_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICEBOUNDSESSIONS_DEVICE_BOUND_SESSION_MANAGER_SUPPLEMENT_H_

#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/restricted_device_bound_sessions.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DeviceBoundSessionRegistrationObserver;
class LocalDOMWindow;

// Does the Mojo work for every `DeviceBoundSessionRegistrationObserver` of a
// window. Holds the window's one `RestrictedDeviceBoundSessionManager` remote,
// so a Document opens a single pipe to the Network Service however many
// observers script creates. Each `observe()` registers here: the supplement
// subscribes with an observer endpoint of its own on that pipe, passes the
// subscription's sessions to the observer, and holds the observer until the
// subscription ends, which keeps it alive while script holds no reference to
// it.
class MODULES_EXPORT DeviceBoundSessionManagerSupplement final
    : public GarbageCollected<DeviceBoundSessionManagerSupplement>,
      public Supplement<LocalDOMWindow>,
      public ExecutionContextLifecycleObserver,
      public network::mojom::blink::RestrictedDeviceBoundSessionObserver {
 public:
  static constexpr char kSupplementName[] =
      "DeviceBoundSessionManagerSupplement";

  static DeviceBoundSessionManagerSupplement& From(LocalDOMWindow& window);

  explicit DeviceBoundSessionManagerSupplement(LocalDOMWindow& window);

  DeviceBoundSessionManagerSupplement(
      const DeviceBoundSessionManagerSupplement&) = delete;
  DeviceBoundSessionManagerSupplement& operator=(
      const DeviceBoundSessionManagerSupplement&) = delete;

  // Subscribes `observer` to this window's sessions and returns the
  // subscription's id, which no other subscription of the window has.
  // Callers must first check that
  // `LocalDOMWindow::CanUseDeviceBoundSessionJsApi()` is true.
  mojo::ReceiverId RegisterObserver(
      DeviceBoundSessionRegistrationObserver* observer);

  // Ends subscription `id`. A reply to it that is still on its way is dropped.
  void UnregisterObserver(mojo::ReceiverId id);

  // Whether subscription `id` is live. `UnregisterObserver()`, a closed
  // endpoint and the destruction of the context each end it.
  bool IsRegistered(mojo::ReceiverId id) const;

  bool HasObserverForTesting(
      DeviceBoundSessionRegistrationObserver* observer) const;

  // ExecutionContextLifecycleObserver:
  void ContextDestroyed() override;

  void Trace(Visitor* visitor) const override;

 private:
  // network::mojom::blink::RestrictedDeviceBoundSessionObserver:
  void OnDeviceBoundSessionRegistered(
      network::mojom::blink::RestrictedDeviceBoundSessionPtr session) override;

  // Passes the reply of subscription `id` to its observer, unless the
  // subscription has ended.
  void OnExistingSessionsRetrieved(
      mojo::ReceiverId id,
      Vector<network::mojom::blink::RestrictedDeviceBoundSessionPtr>
          existing_sessions);

  // Ends the subscription whose endpoint closed.
  void OnEndpointDisconnected();

  // Binds the remote through the BrowserInterfaceBroker on first use and after
  // its pipe closes, and returns it.
  network::mojom::blink::RestrictedDeviceBoundSessionManager* GetManager();

  HeapMojoRemote<network::mojom::blink::RestrictedDeviceBoundSessionManager>
      manager_;

  // One endpoint per subscription, associated with `manager_` and bound on the
  // same task runner, so pushes and the reply arrive in the order they were
  // sent.
  HeapMojoAssociatedReceiverSet<
      network::mojom::blink::RestrictedDeviceBoundSessionObserver,
      DeviceBoundSessionManagerSupplement>
      receivers_;

  // The observer of each live subscription, keyed by its id in `receivers_`.
  // Strong references, so each observing observer stays alive.
  HeapHashMap<mojo::ReceiverId, Member<DeviceBoundSessionRegistrationObserver>>
      observers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICEBOUNDSESSIONS_DEVICE_BOUND_SESSION_MANAGER_SUPPLEMENT_H_
