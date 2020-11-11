// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compression/deflate_transformer.h"

#include <string.h>
#include <algorithm>
#include <limits>

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/compression/compression_format.h"
#include "third_party/blink/renderer/modules/compression/zlib_partition_alloc.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
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
  }
  DCHECK_EQ(Z_OK, err);
}

DeflateTransformer::~DeflateTransformer() {
  if (!was_flush_called_) {
    deflateEnd(&stream_);
  }
}

ScriptPromise DeflateTransformer::Transform(
    v8::Local<v8::Value> chunk,
    TransformStreamDefaultController* controller,
    ExceptionState& exception_state) {
  ArrayBufferOrArrayBufferView buffer_source;
  V8ArrayBufferOrArrayBufferView::ToImpl(
      script_state_->GetIsolate(), chunk, buffer_source,
      UnionTypeConversionMode::kNotNullable, exception_state);
  if (exception_state.HadException()) {
    return ScriptPromise();
  }
  if (buffer_source.IsArrayBufferView()) {
    const auto* view = buffer_source.GetAsArrayBufferView().View();
    const uint8_t* start = static_cast<const uint8_t*>(view->BaseAddress());
    size_t length = view->byteLength();
    if (length > std::numeric_limits<wtf_size_t>::max()) {
      exception_state.ThrowRangeError(
          "Buffer size exceeds maximum heap object size.");
      return ScriptPromise();
    }
    Deflate(start, static_cast<wtf_size_t>(length), IsFinished(false),
            controller, exception_state);
    return ScriptPromise::CastUndefined(script_state_);
  }
  DCHECK(buffer_source.IsArrayBuffer());
  const auto* array_buffer = buffer_source.GetAsArrayBuffer();
  const uint8_t* start = static_cast<const uint8_t*>(array_buffer->Data());
  size_t length = array_buffer->ByteLength();
  if (length > std::numeric_limits<wtf_size_t>::max()) {
    exception_state.ThrowRangeError(
        "Buffer size exceeds maximum heap object size.");
    return ScriptPromise();
  }
  Deflate(start, static_cast<wtf_size_t>(length), IsFinished(false), controller,
          exception_state);

  return ScriptPromise::CastUndefined(script_state_);
}

ScriptPromise DeflateTransformer::Flush(
    TransformStreamDefaultController* controller,
    ExceptionState& exception_state) {
  Deflate(nullptr, 0u, IsFinished(true), controller, exception_state);
  was_flush_called_ = true;
  deflateEnd(&stream_);
  out_buffer_.clear();

  return ScriptPromise::CastUndefined(script_state_);
}

void DeflateTransformer::Deflate(const uint8_t* start,
                                 wtf_size_t length,
                                 IsFinished finished,
                                 TransformStreamDefaultController* controller,
                                 ExceptionState& exception_state) {
  stream_.avail_in = length;
  // Zlib treats this pointer as const, so this cast is safe.
  stream_.next_in = const_cast<uint8_t*>(start);

  do {
    stream_.avail_out = out_buffer_.size();
    stream_.next_out = out_buffer_.data();
    int err = deflate(&stream_, finished ? Z_FINISH : Z_NO_FLUSH);
    DCHECK((finished && err == Z_STREAM_END) || err == Z_OK ||
           err == Z_BUF_ERROR);

    wtf_size_t bytes = out_buffer_.size() - stream_.avail_out;
    if (bytes) {
      controller->enqueue(
          script_state_,
          ScriptValue::From(script_state_,
                            DOMUint8Array::Create(out_buffer_.data(), bytes)),
          exception_state);
      if (exception_state.HadException()) {
        return;
      }
    }
  } while (stream_.avail_out == 0);
}

void DeflateTransformer::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  TransformStreamTransformer::Trace(visitor);
}

}  // namespace blink
