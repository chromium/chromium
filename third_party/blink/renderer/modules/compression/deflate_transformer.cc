// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compression/deflate_transformer.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include "base/trace_event/typed_macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/compression/compression_format.h"
#include "third_party/blink/renderer/modules/compression/zlib_partition_alloc.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "v8/include/v8.h"

namespace blink {

DeflateTransformer::DeflateTransformer(ScriptState* script_state,
                                       CompressionFormat format,
                                       int level)
    : script_state_(script_state), out_buffer_(kBufferSize) {
  DCHECK(level >= 1 && level <= 9);
  memset(&stream_, 0, sizeof(z_stream));
  ZlibPartitionAlloc::Configure(&stream_);
  constexpr int kWindowBits = 15;
  constexpr int kUseGzip = 16;
  int err;
  switch (format) {
    case CompressionFormat::kDeflate:
      err = deflateInit2(&stream_, level, Z_DEFLATED, kWindowBits, 8,
                         Z_DEFAULT_STRATEGY);
      break;
    case CompressionFormat::kGzip:
      err = deflateInit2(&stream_, level, Z_DEFLATED, kWindowBits + kUseGzip, 8,
                         Z_DEFAULT_STRATEGY);
      break;
    case CompressionFormat::kDeflateRaw:
      err = deflateInit2(&stream_, level, Z_DEFLATED, -kWindowBits, 8,
                         Z_DEFAULT_STRATEGY);
      break;
  }
  DCHECK_EQ(Z_OK, err);
}

DeflateTransformer::~DeflateTransformer() {
  if (!was_flush_called_) {
    deflateEnd(&stream_);
  }
}

ScriptPromise<IDLUndefined> DeflateTransformer::Transform(
    v8::Local<v8::Value> chunk,
    TransformStreamDefaultController* controller,
    ExceptionState& exception_state) {
  auto* buffer_source = V8BufferSource::Create(script_state_->GetIsolate(),
                                               chunk, exception_state);
  if (exception_state.HadException())
    return EmptyPromise();
  DOMArrayPiece array_piece(buffer_source);
  if (array_piece.ByteLength() > std::numeric_limits<wtf_size_t>::max()) {
    exception_state.ThrowRangeError(
        "Buffer size exceeds maximum heap object size.");
    return EmptyPromise();
  }
  Deflate(array_piece.Bytes(),
          static_cast<wtf_size_t>(array_piece.ByteLength()), IsFinished(false),
          controller, exception_state);
  return ToResolvedUndefinedPromise(script_state_.Get());
}

ScriptPromise<IDLUndefined> DeflateTransformer::Flush(
    TransformStreamDefaultController* controller,
    ExceptionState& exception_state) {
  Deflate(nullptr, 0u, IsFinished(true), controller, exception_state);
  was_flush_called_ = true;
  deflateEnd(&stream_);
  out_buffer_.clear();

  return ToResolvedUndefinedPromise(script_state_.Get());
}

void DeflateTransformer::Deflate(const uint8_t* start,
                                 wtf_size_t length,
                                 IsFinished finished,
                                 TransformStreamDefaultController* controller,
                                 ExceptionState& exception_state) {
  TRACE_EVENT("blink,devtools.timeline", "CompressionStream Deflate");
  stream_.avail_in = length;
  // Zlib treats this pointer as const, so this cast is safe.
  stream_.next_in = const_cast<uint8_t*>(start);

  // enqueue() may execute JavaScript which may invalidate the input buffer. So
  // accumulate all the output before calling enqueue().
  HeapVector<Member<DOMUint8Array>, 1u> buffers;

  do {
    stream_.avail_out = out_buffer_.size();
    stream_.next_out = out_buffer_.data();
    int err = deflate(&stream_, finished ? Z_FINISH : Z_NO_FLUSH);
    DCHECK((finished && err == Z_STREAM_END) || err == Z_OK ||
           err == Z_BUF_ERROR);

    wtf_size_t bytes = out_buffer_.size() - stream_.avail_out;
    if (bytes) {
      buffers.push_back(
          DOMUint8Array::Create(base::span(out_buffer_).first(bytes)));
    }
  } while (stream_.avail_out == 0);

  DCHECK_EQ(stream_.avail_in, 0u);

  // JavaScript may be executed inside this loop, however it is safe because
  // |buffers| is a local variable that JavaScript cannot modify.
  for (DOMUint8Array* buffer : buffers) {
    controller->enqueue(script_state_, ScriptValue::From(script_state_, buffer),
                        exception_state);
    if (exception_state.HadException()) {
      return;
    }
  }
}

void DeflateTransformer::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  TransformStreamTransformer::Trace(visitor);
}

}  // namespace blink
