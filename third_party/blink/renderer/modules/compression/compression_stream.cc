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
  return transform_->Readable();
}

WritableStream* CompressionStream::writable() const {
  return transform_->Writable();
}

void CompressionStream::Trace(Visitor* visitor) const {
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
}

CompressionStream::CompressionStream(ScriptState* script_state,
                                     const AtomicString& format,
                                     ExceptionState& exception_state) {
  CHECK(exception_state.GetIsolate());

  static auto* const compression_stream_deflate_format = AllocateCrashKeyString(
      "compression_stream_deflate_format", base::debug::CrashKeySize::Size32);
  SetCrashKeyString(compression_stream_deflate_format, format.Utf8());
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
  if (exception_state.HadException()) {
    return;
  }
  CHECK(transform_);
}

}  // namespace blink
