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
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
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
      length, blink::BindOnce(&CrashReportStorage::OnCreateCrashReportStorage,
                              WrapPersistent(this),
                              WrapPersistent(resolver_.Get()), length));
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

  storage_.Set(key, value);
  if (!CheckSizeAndWriteKey(key, value, exception_state)) {
    // If the write failed, this is because the `key`/`value` pair was too large
    // for the requested memory buffer; undo the insertion.
    storage_.erase(key);
  }
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

  storage_.erase(key);
  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().RemoveCrashReportStorageKey(key);
}

void CrashReportStorage::OnCreateCrashReportStorage(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    uint64_t length) {
  initialization_complete_ = true;
  length_ = length;
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

bool CrashReportStorage::CheckSizeAndWriteKey(const String& key,
                                              const String& value,
                                              ExceptionState& exception_state) {
  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame->GetDocument());

  auto json_object = std::make_unique<JSONObject>();
  for (const auto& it : storage_) {
    json_object->SetString(it.key, it.value);
  }

  // Serialize the map as a JSON-ified string to test length here, even though
  // the browser just stores this in a normal `std::map`, before serializing it
  // into a `base::Value::Dict` for the actual crash report. It doesn't matter
  // if these serializations are equivalent; what matters is that the
  // serialization chosen for web-exposed length enforcement is consistent.
  //
  // Regardless, the length enforcement done here is not load-bearing for
  // security; it's *only* for web-exposed behavior. Right now, the browser does
  // not enforce length, but this will all change when we move to the shared
  // memory model being implemented in https://crrev.com/c/6788146.
  String json_string = json_object->ToJSONString();
  StringUtf8Adaptor utf8(json_string);

  // Compute whether the total size of the JSON-ified data that will need to be
  // written to browser memory, in bytes, is larger than the requested
  // `length_`.
  if (!base::IsValueInRangeForNumericType<uint32_t>(utf8.size()) ||
      utf8.size() > length_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "The crash report data is too large to "
                                      "be stored in the requested buffer.");
    return false;
  }

  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().SetCrashReportStorageKey(key, value);
  return true;
}

}  // namespace blink
