// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICEBOUNDSESSIONS_DEVICE_BOUND_SESSION_REGISTRATION_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICEBOUNDSESSIONS_DEVICE_BOUND_SESSION_REGISTRATION_OBSERVER_H_

#include <optional>

#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/restricted_device_bound_sessions.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DeviceBoundSession;
class DeviceBoundSessionManagerSupplement;
class DeviceBoundSessionRegistrationObserverOptions;
class ExceptionState;
class ExecutionContext;
class ScriptState;
class V8DeviceBoundSessionRegistrationObserverCallback;

// Implements the `DeviceBoundSessionRegistrationObserver` WebIDL interface. It
// reports the device bound sessions this document can see: the existing ones,
// if any, in one callback after `observe()`, then each new one in its own
// callback.
class MODULES_EXPORT DeviceBoundSessionRegistrationObserver final
    : public ScriptWrappable,
      public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using PassKey = base::PassKey<DeviceBoundSessionRegistrationObserver>;

  // Throws an `InvalidStateError` in a destroyed context and a `SecurityError`
  // for an opaque origin, before anything is bound.
  static DeviceBoundSessionRegistrationObserver* Create(
      ScriptState* script_state,
      V8DeviceBoundSessionRegistrationObserverCallback* callback,
      const DeviceBoundSessionRegistrationObserverOptions* options,
      ExceptionState& exception_state);

  explicit DeviceBoundSessionRegistrationObserver(
      PassKey,
      ExecutionContext* execution_context,
      V8DeviceBoundSessionRegistrationObserverCallback* callback,
      const DeviceBoundSessionRegistrationObserverOptions* options);

  DeviceBoundSessionRegistrationObserver(
      const DeviceBoundSessionRegistrationObserver&) = delete;
  DeviceBoundSessionRegistrationObserver& operator=(
      const DeviceBoundSessionRegistrationObserver&) = delete;

  // WebIDL methods.

  // Registers with the window's supplement, which subscribes to the sessions
  // and passes them to `OnDeviceBoundSessionRegistered()` and
  // `OnExistingSessionsRetrieved()`.
  void observe();

  // Stops delivery and ends this observer's subscription. The `AbortSignal`
  // path uses `StopObserving()` directly so that it records only `kAborted`.
  void disconnect();

  // Oilpan tracing.
  void Trace(Visitor* visitor) const override;

  // Called by the supplement for each session pushed to this observer's
  // subscription. Converts the session into a WebIDL `DeviceBoundSession` and
  // invokes the JS callback, or holds it until the snapshot reply arrives.
  // Does no other filtering; the Network Service has already scoped the
  // session to this Document.
  void OnDeviceBoundSessionRegistered(
      network::mojom::blink::RestrictedDeviceBoundSessionPtr session);

  // Called by the supplement with the snapshot reply to this observer's
  // subscription. Converts each entry into a WebIDL `DeviceBoundSession`, adds
  // the held pushes whose id is not in the reply, and invokes the JS callback
  // once if the list is not empty.
  void OnExistingSessionsRetrieved(
      Vector<network::mojom::blink::RestrictedDeviceBoundSessionPtr>
          existing_sessions);

 private:
  // Whether the supplement still has this observer's subscription. The
  // supplement also ends it on its own when the endpoint closes or the context
  // is destroyed.
  bool IsObserving() const;

  // Registered against the supplied `AbortSignal`. Records the abort metric and
  // stops observing for good. The callback is not invoked.
  void OnAbort();

  // Ends the subscription with the supplement. Keeps `abort_handle_`, so an
  // abort after a later `observe()` still stops the observer. The shared
  // manager remote on the supplement stays bound.
  void StopObserving();

  // The window's supplement. It holds the manager remote, so the pipe to the
  // Network Service is shared across the Document, and it holds this observer
  // while it observes.
  Member<DeviceBoundSessionManagerSupplement> supplement_;

  // Cleared on abort, so an aborted observer can never call script again,
  // even if `observe()` is called after the abort.
  Member<V8DeviceBoundSessionRegistrationObserverCallback> callback_;

  // Kept for the observer's life: `AbortSignal` holds this handle only weakly,
  // and drops the abort algorithm once a GC collects it.
  Member<AbortSignal::AlgorithmHandle> abort_handle_;

  // A push sent before the snapshot reply arrives before it. Such pushes are
  // held here and merged into the reply, without duplicates.
  bool did_receive_snapshot_ = false;
  HeapVector<Member<DeviceBoundSession>> pre_snapshot_sessions_;

  // The id the supplement gave this observer's latest subscription.
  std::optional<mojo::ReceiverId> subscription_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_DEVICEBOUNDSESSIONS_DEVICE_BOUND_SESSION_REGISTRATION_OBSERVER_H_
