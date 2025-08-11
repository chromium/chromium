// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/crash_report_storage.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {}  // namespace

CrashReportStorage::CrashReportStorage(LocalDOMWindow& window)
    : ExecutionContextClient(&window) {
  DCHECK(RuntimeEnabledFeatures::CrashReportingStorageAPIEnabled(
      GetExecutionContext()));
}

void CrashReportStorage::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);

  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

ScriptPromise<IDLUndefined> CrashReportStorage::initialize(
    ScriptState* script_state,
    uint64_t length,
    ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot use CrashReportStorage with a "
                                      "document that is not fully active.");
    return ScriptPromise<IDLUndefined>();
  }

  if (resolver_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The initialize() method has already been called.");
    return ScriptPromise<IDLUndefined>();
  }

  resolver_ = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  ScriptPromise<IDLUndefined> promise = resolver_->Promise();

  if (length > mojom::blink::kMaxCrashReportStorageSize) {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                           "The requested size is too large."));
    return promise;
  }

  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame);

  frame->GetLocalFrameHostRemote().InitializeCrashReportStorage(
      length,
      blink::BindOnce(&CrashReportStorage::OnCreateCrashReportStorage,
                      WrapPersistent(this), WrapPersistent(resolver_.Get())));
  return promise;
}

void CrashReportStorage::set(const String& key,
                             const String& value,
                             ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot use CrashReportStorage with a "
                                      "document that is not fully active.");
    return;
  }

  if (!initialization_complete_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "CrashReportStorage is not initialized. Call initialize() and wait for "
        "it to resolve.");
    return;
  }

  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame->GetDocument());

  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().SetCrashReportStorageKey(key, value);
}

void CrashReportStorage::remove(const String& key,
                                ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot use CrashReportStorage with a "
                                      "document that is not fully active.");
    return;
  }

  if (!initialization_complete_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "CrashReportStorage is not initialized. Call initialize() and wait for "
        "it to resolve.");
    return;
  }

  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame->GetDocument());

  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().RemoveCrashReportStorageKey(key);
}

void CrashReportStorage::OnCreateCrashReportStorage(
    ScriptPromiseResolver<IDLUndefined>* resolver) {
  initialization_complete_ = true;
  // Trivially resolve `resolver`. The reason this API has the Promise-returning
  // `initialize()` method in the first place is to provide an asynchronous
  // window for the implementation—in this case, the browser process—to
  // initialize whatever backing memory mechanism is appropriate to store this
  // API's inputs.
  //
  // In the future, this method may be more complicated if we move forward with
  // an implementation based off of shared memory. See
  // https://crrev.com/c/6788146.
  resolver->Resolve();
}

}  // namespace blink
