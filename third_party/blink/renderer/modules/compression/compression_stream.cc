// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compression/compression_stream.h"

#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/modules/compression/compression_format.h"
#include "third_party/blink/renderer/modules/compression/deflate_transformer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "v8/include/v8-sandbox.h"

namespace blink {

CompressionStream* CompressionStream::Create(ScriptState* script_state,
                                             const AtomicString& format,
                                             ExceptionState& exception_state) {
  return MakeGarbageCollected<CompressionStream>(script_state, format,
                                                 exception_state);
}

ReadableStream* CompressionStream::readable() const {
  CHECK(initialized_);
  return transform_->Readable();
}

WritableStream* CompressionStream::writable() const {
  CHECK(initialized_);
  return transform_->Writable();
}

void CompressionStream::Trace(Visitor* visitor) const {
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
}

CompressionStream::CompressionStream(ScriptState* script_state,
                                     const AtomicString& format,
                                     ExceptionState& exception_state) {
  CompressionFormat deflate_format =
      LookupCompressionFormat(format, exception_state);
  if (exception_state.HadException())
    return;

  UMA_HISTOGRAM_ENUMERATION("Blink.Compression.CompressionStream.Format",
                            deflate_format);

  // default level is hardcoded for now.
  // TODO(arenevier): Make level configurable
  const int deflate_level = 6;
  transform_ =
      TransformStream::Create(script_state,
                              MakeGarbageCollected<DeflateTransformer>(
                                  script_state, deflate_format, deflate_level),
                              exception_state);
  CHECK(transform_);
  initialized_ = true;
}

namespace bindings {

// TODO(427166012): remove once we're done with troubleshooting.
void ReceiverValidatorForDebugging<CompressionStream>::Validate(
    v8::Isolate* isolate,
    v8::Local<v8::Object> object,
    CompressionStream* receiver) {
  if (receiver) {
    return;
  }

  CHECK(!object.IsEmpty());
  CHECK(!object->IsNull());
  static auto* const script_url =
      AllocateCrashKeyString("script_url", base::debug::CrashKeySize::Size256);
  SetCrashKeyString(script_url, CaptureCurrentScriptUrl(isolate).Utf8());

  v8::MaybeLocal<v8::Context> creation_context =
      object->GetCreationContext(isolate);
  v8::MaybeLocal<v8::String> as_string =
      !creation_context.IsEmpty()
          ? object->ObjectProtoToString(creation_context.ToLocalChecked())
          : v8::MaybeLocal<v8::String>();
  if (!as_string.IsEmpty()) {
    static auto* const object_to_string = AllocateCrashKeyString(
        "object_to_string", base::debug::CrashKeySize::Size256);
    SetCrashKeyString(object_to_string,
                      ToBlinkString<String>(isolate, as_string.ToLocalChecked(),
                                            kDoNotExternalize)
                          .Utf8());
  }

  static auto* const constructor =
      AllocateCrashKeyString("constructor", base::debug::CrashKeySize::Size64);
  SetCrashKeyString(constructor,
                    ToBlinkString<String>(isolate, object->GetConstructorName(),
                                          kDoNotExternalize)
                        .Utf8());

  static auto* const wrappable =
      AllocateCrashKeyString("wrappable", base::debug::CrashKeySize::Size32);
  SetCrashKeyString(
      wrappable,
      String::Format(
          "0x%p", v8::Object::Unwrap(isolate, object, v8::kAnyCppHeapPointer))
          .Utf8());

  const bool is_same_context =
      !creation_context.IsEmpty() &&
      creation_context.ToLocalChecked() == isolate->GetCurrentContext();
  static auto* const same_context =
      AllocateCrashKeyString("same_context", base::debug::CrashKeySize::Size32);
  SetCrashKeyString(same_context, is_same_context ? "true" : "false");

  NOTREACHED();
}
}  // namespace bindings

}  // namespace blink
