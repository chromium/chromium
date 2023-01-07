// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compression/compression_stream.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/modules/compression/compression_format.h"
#include "third_party/blink/renderer/modules/compression/deflate_transformer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

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
}

}  // namespace blink
