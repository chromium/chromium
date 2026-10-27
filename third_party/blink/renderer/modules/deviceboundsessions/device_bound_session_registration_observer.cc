// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/deviceboundsessions/device_bound_session_registration_observer.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_device_bound_session.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_device_bound_session_registration_observer_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_device_bound_session_registration_observer_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/deviceboundsessions/device_bound_session_manager_supplement.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(RegistrationObserverAbortedStatus)
enum class RegistrationObserverAbortedStatus {
  // Script called `disconnect()` on an observer that was observing.
  kCompleted = 0,
  // Observer was aborted via `AbortSignal`.
  kAborted = 1,
  kMaxValue = kAborted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:RegistrationObserverAbortedStatus)

void RecordAbortedStatus(RegistrationObserverAbortedStatus status) {
  base::UmaHistogramEnumeration("Blink.DeviceBoundSession.AbortedStatus",
                                status);
}

DeviceBoundSession* ToDeviceBoundSession(
    const network::mojom::blink::RestrictedDeviceBoundSession& session) {
  DeviceBoundSession* blink_session = DeviceBoundSession::Create();
  blink_session->setSessionId(session.session_id);
  blink_session->setRefreshURL(session.refresh_url.GetString());
  blink_session->setCreationTime(
      session.creation_time.InMillisecondsFSinceUnixEpoch());
  return blink_session;
}

}  // namespace

// static
DeviceBoundSessionRegistrationObserver*
DeviceBoundSessionRegistrationObserver::Create(
    ScriptState* script_state,
    V8DeviceBoundSessionRegistrationObserverCallback* callback,
    const DeviceBoundSessionRegistrationObserverOptions* options,
    ExceptionState& exception_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // A removed iframe's window can still run script after its context is
  // destroyed. An observer made then would never learn of the destruction.
  if (!execution_context || execution_context->IsContextDestroyed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "No execution context.");
    return nullptr;
  }

  // A sandboxed iframe or a data: document gets an exception it can handle.
  // The browser binder treats a request from such a context as a bad message.
  if (execution_context->GetSecurityOrigin()->IsOpaque()) {
    exception_state.ThrowSecurityError(
        "Device bound sessions are not available to an opaque origin.");
    return nullptr;
  }

  return MakeGarbageCollected<DeviceBoundSessionRegistrationObserver>(
      PassKey(), execution_context, callback, options);
}

DeviceBoundSessionRegistrationObserver::DeviceBoundSessionRegistrationObserver(
    PassKey,
    ExecutionContext* execution_context,
    V8DeviceBoundSessionRegistrationObserverCallback* callback,
    const DeviceBoundSessionRegistrationObserverOptions* options)
    : ExecutionContextClient(execution_context),
      supplement_(&DeviceBoundSessionManagerSupplement::From(
          *To<LocalDOMWindow>(execution_context))),
      callback_(callback) {
  if (options->hasSignal()) {
    AbortSignal* signal = options->signal();
    if (signal->aborted()) {
      callback_.Clear();
      RecordAbortedStatus(RegistrationObserverAbortedStatus::kAborted);
    } else {
      abort_handle_ = signal->AddAlgorithm(
          BindOnce(&DeviceBoundSessionRegistrationObserver::OnAbort,
                   WrapWeakPersistent(this)));
    }
  }
}

void DeviceBoundSessionRegistrationObserver::observe() {
  LocalDOMWindow* window = DomWindow();
  if (!window || !callback_ || IsObserving()) {
    return;
  }

  // Like `USB::getDevices()`, check before connecting. The browser decided at
  // commit whether this document may use the API, and reports a bad message
  // if it asks anyway. The callback runs only for a non-empty list, so script
  // cannot tell this from having no sessions.
  if (!window->CanUseDeviceBoundSessionJsApi()) {
    return;
  }

  did_receive_snapshot_ = false;
  // The supplement ends a subscription whose endpoint closed without calling
  // this observer, so drop what was held for an earlier subscription here.
  pre_snapshot_sessions_.clear();
  subscription_id_ = supplement_->RegisterObserver(this);
}

void DeviceBoundSessionRegistrationObserver::disconnect() {
  if (IsObserving()) {
    RecordAbortedStatus(RegistrationObserverAbortedStatus::kCompleted);
  }
  StopObserving();
}

void DeviceBoundSessionRegistrationObserver::Trace(Visitor* visitor) const {
  visitor->Trace(supplement_);
  visitor->Trace(callback_);
  visitor->Trace(abort_handle_);
  visitor->Trace(pre_snapshot_sessions_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void DeviceBoundSessionRegistrationObserver::OnDeviceBoundSessionRegistered(
    network::mojom::blink::RestrictedDeviceBoundSessionPtr session) {
  if (!did_receive_snapshot_) {
    pre_snapshot_sessions_.push_back(ToDeviceBoundSession(*session));
    return;
  }
  HeapVector<Member<DeviceBoundSession>> records = {
      ToDeviceBoundSession(*session)};
  callback_->InvokeAndReportException(this, records, this);
}

void DeviceBoundSessionRegistrationObserver::OnExistingSessionsRetrieved(
    Vector<network::mojom::blink::RestrictedDeviceBoundSessionPtr>
        existing_sessions) {
  did_receive_snapshot_ = true;

  HeapVector<Member<DeviceBoundSession>> records;
  for (const auto& session : existing_sessions) {
    records.push_back(ToDeviceBoundSession(*session));
  }
  // A held push can also be in the reply when the session was stored while
  // the Network Service was loading its store.
  for (const Member<DeviceBoundSession>& held : pre_snapshot_sessions_) {
    if (!std::ranges::contains(records, held->sessionId(),
                               &DeviceBoundSession::sessionId)) {
      records.push_back(held);
    }
  }
  pre_snapshot_sessions_.clear();

  if (!records.empty()) {
    callback_->InvokeAndReportException(this, records, this);
  }
}

bool DeviceBoundSessionRegistrationObserver::IsObserving() const {
  return subscription_id_ && supplement_->IsRegistered(*subscription_id_);
}

void DeviceBoundSessionRegistrationObserver::OnAbort() {
  RecordAbortedStatus(RegistrationObserverAbortedStatus::kAborted);
  callback_.Clear();
  StopObserving();
}

void DeviceBoundSessionRegistrationObserver::StopObserving() {
  if (subscription_id_) {
    supplement_->UnregisterObserver(*subscription_id_);
    subscription_id_.reset();
  }
}

}  // namespace blink
