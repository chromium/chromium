// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/crash_report_context.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CrashReportContext::CrashReportContext(LocalDOMWindow& window)
    : ExecutionContextClient(&window) {
  DCHECK(RuntimeEnabledFeatures::CrashReportingStorageAPIEnabled(
      GetExecutionContext()));
}

void CrashReportContext::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);

  ScriptWrappable::Trace(visitor);
  visitor->Trace(resolver_);
  ExecutionContextClient::Trace(visitor);
}

ScriptPromise<IDLUndefined> CrashReportContext::initialize(
    ScriptState* script_state,
    uint64_t length,
    ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot use CrashReportContext with a "
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

  if (length > mojom::blink::kMaxCrashReportContextSize) {
    resolver_->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kNotAllowedError,
                                           "The requested size is too large."));
    return promise;
  }

  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame);

  frame->GetLocalFrameHostRemote().InitializeCrashReportContext(
      length,
      blink::BindOnce(&CrashReportContext::OnCreateCrashReportContext,
                      WrapPersistent(this), WrapPersistent(resolver_.Get())));
  return promise;
}

void CrashReportContext::set(const String& key,
                             const String& value,
                             ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot use CrashReportContext with a "
                                      "document that is not fully active.");
    return;
  }

  if (!shm_mapping_.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "CrashReportContext is not initialized. Call initialize() and wait for "
        "it to resolve.");
    return;
  }

  storage_.Set(key, value);
  if (!WriteToSharedMemory(exception_state)) {
    // If the write failed, this is because the `key`/`value` pair was too large
    // for the requested memory buffer; undo the insertion.
    storage_.erase(key);
  }
}

void CrashReportContext::deleteKey(const String& key,
                                   ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot use CrashReportContext with a "
                                      "document that is not fully active.");
    return;
  }

  if (!shm_mapping_.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "CrashReportContext is not initialized. Call initialize() and wait for "
        "it to resolve.");
    return;
  }

  storage_.erase(key);
  WriteToSharedMemory(exception_state);
}

void CrashReportContext::OnCreateCrashReportContext(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    base::UnsafeSharedMemoryRegion region) {
  // The mapping must not have been already initialized. See
  // `CrashReportContext::initialize()`.
  CHECK(!shm_mapping_.IsValid());

  if (!region.IsValid()) {
    resolver->Reject();
    return;
  }
  shm_mapping_ = region.Map();
  if (!shm_mapping_.IsValid()) {
    resolver->Reject();
    return;
  }
  resolver->Resolve();
}

bool CrashReportContext::WriteToSharedMemory(ExceptionState& exception_state) {
  CHECK(shm_mapping_.IsValid());

  auto json_object = std::make_unique<JSONObject>();
  for (const auto& it : storage_) {
    json_object->SetString(it.key, it.value);
  }

  String json_string = json_object->ToJSONString();
  StringUtf8Adaptor utf8(json_string);

  // Compute the total size of the data that will need to be written to shared
  // memory, in bytes. This includes:
  //   1. The actual JSONified key-value data, and
  //   2. A leading uint32_t that tells the reader how many bytes of data have
  //      been written, after the lead integer.
  base::CheckedNumeric<size_t> total_size = utf8.size();
  total_size += sizeof(uint32_t);

  if (base::CheckAdd(utf8.size(), sizeof(uint32_t))
          .IsInvalidOr(
              [this](uint32_t val) { return val > shm_mapping_.size(); })) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      "The crash report data is too large to "
                                      "be stored in the requested buffer.");
    return false;
  }

  base::span<uint8_t> buffer = shm_mapping_.GetMemoryAsSpan<uint8_t>();
  base::SpanWriter writer(buffer);

  // Write the leading integer which tells the reader how many bytes of report
  // data are written after the said integer.
  writer.WriteU32NativeEndian(utf8.size());

  // Write the JSONified body to the buffer.
  writer.Write(base::as_bytes(base::as_byte_span(utf8.AsStringView())));
  return true;
}

}  // namespace blink
