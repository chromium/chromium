// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compression/inflate_transformer.h"

#include <string.h>
#include <algorithm>
#include <limits>

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_uint8_array.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/modules/compression/compression_format.h"
#include "third_party/blink/renderer/modules/compression/zlib_partition_alloc.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "v8/include/v8.h"

namespace blink {

InflateTransformer::InflateTransformer(ScriptState* script_state,
                                       CompressionFormat format)
    : script_state_(script_state), out_buffer_(kBufferSize) {
  memset(&stream_, 0, sizeof(z_stream));
  ZlibPartitionAlloc::Configure(&stream_);
  constexpr int kWindowBits = 15;
  constexpr int kUseGzip = 16;
  int err;
  switch (format) {
    case CompressionFormat::kDeflate:
      err = inflateInit2(&stream_, kWindowBits);
      break;
    case CompressionFormat::kGzip:
      err = inflateInit2(&stream_, kWindowBits + kUseGzip);
      break;
  }
  DCHECK_EQ(Z_OK, err);
}

InflateTransformer::~InflateTransformer() {
  if (!was_flush_called_) {
    inflateEnd(&stream_);
  }
}

ScriptPromise InflateTransformer::Transform(
    v8::Local<v8::Value> chunk,
    TransformStreamDefaultControllerInterface* controller,
    ExceptionState& exception_state) {
  // TODO(canonmukai): Support SharedArrayBuffer.
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
    wtf_size_t length = view->byteLength();
    Inflate(start, length, IsFinished(false), controller, exception_state);
    return ScriptPromise::CastUndefined(script_state_);
  }
  DCHECK(buffer_source.IsArrayBuffer());
  const auto* array_buffer = buffer_source.GetAsArrayBuffer();
  const uint8_t* start = static_cast<const uint8_t*>(array_buffer->Data());
  wtf_size_t length = array_buffer->DeprecatedByteLengthAsUnsigned();
  Inflate(start, length, IsFinished(false), controller, exception_state);

  return ScriptPromise::CastUndefined(script_state_);
}

ScriptPromise InflateTransformer::Flush(
    TransformStreamDefaultControllerInterface* controller,
    ExceptionState& exception_state) {
  DCHECK(!was_flush_called_);
  Inflate(nullptr, 0u, IsFinished(true), controller, exception_state);
  inflateEnd(&stream_);
  was_flush_called_ = true;
  out_buffer_.clear();

  return ScriptPromise::CastUndefined(script_state_);
}

void InflateTransformer::Inflate(
    const uint8_t* start,
    wtf_size_t length,
    IsFinished finished,
    TransformStreamDefaultControllerInterface* controller,
    ExceptionState& exception_state) {
  stream_.avail_in = length;
  // Zlib treats this pointer as const, so this cast is safe.
  stream_.next_in = const_cast<uint8_t*>(start);

  do {
    stream_.avail_out = out_buffer_.size();
    stream_.next_out = out_buffer_.data();
    int err = inflate(&stream_, finished ? Z_FINISH : Z_NO_FLUSH);
    if (err != Z_OK && err != Z_STREAM_END && err != Z_BUF_ERROR) {
      exception_state.ThrowTypeError("The compressed data was not valid.");
      return;
    }

    wtf_size_t bytes = out_buffer_.size() - stream_.avail_out;
    if (bytes) {
      controller->Enqueue(
          ToV8(DOMUint8Array::Create(out_buffer_.data(), bytes), script_state_),
          exception_state);
      if (exception_state.HadException()) {
        return;
      }
    }
  } while (stream_.avail_out == 0);
}

void InflateTransformer::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  TransformStreamTransformer::Trace(visitor);
}

}  // namespace blink
