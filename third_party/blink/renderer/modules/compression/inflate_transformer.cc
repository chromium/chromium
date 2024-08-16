// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compression/inflate_transformer.h"

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
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
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
    case CompressionFormat::kDeflateRaw:
      err = inflateInit2(&stream_, -kWindowBits);
      break;
  }
  DCHECK_EQ(Z_OK, err);
}

InflateTransformer::~InflateTransformer() {
  if (!was_flush_called_) {
    inflateEnd(&stream_);
  }
}

ScriptPromise<IDLUndefined> InflateTransformer::Transform(
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
  Inflate(array_piece.Bytes(),
          static_cast<wtf_size_t>(array_piece.ByteLength()), IsFinished(false),
          controller, exception_state);
  return ToResolvedUndefinedPromise(script_state_.Get());
}

ScriptPromise<IDLUndefined> InflateTransformer::Flush(
    TransformStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK(!was_flush_called_);
  was_flush_called_ = true;
  Inflate(nullptr, 0u, IsFinished(true), controller, exception_state);
  inflateEnd(&stream_);
  out_buffer_.clear();

  if (exception_state.HadException()) {
    return EmptyPromise();
  }

  if (!reached_end_) {
    exception_state.ThrowTypeError("Compressed input was truncated.");
  }

  return ToResolvedUndefinedPromise(script_state_.Get());
}

void InflateTransformer::Inflate(const uint8_t* start,
                                 wtf_size_t length,
                                 IsFinished finished,
                                 TransformStreamDefaultController* controller,
                                 ExceptionState& exception_state) {
  TRACE_EVENT("blink,devtools.timeline", "DecompressionStream Inflate");
  if (reached_end_ && length != 0) {
    // zlib will ignore data after the end of the stream, so we have to
    // explicitly throw an error.
    exception_state.ThrowTypeError("Junk found after end of compressed data.");
    return;
  }

  stream_.avail_in = length;
  // Zlib treats this pointer as const, so this cast is safe.
  stream_.next_in = const_cast<uint8_t*>(start);

  // enqueue() may execute JavaScript which may invalidate the input buffer. So
  // accumulate all the output before calling enqueue().
  HeapVector<Member<DOMUint8Array>, 1u> buffers;

  do {
    stream_.avail_out = out_buffer_.size();
    stream_.next_out = out_buffer_.data();
    const int err = inflate(&stream_, finished ? Z_FINISH : Z_NO_FLUSH);
    if (err != Z_OK && err != Z_STREAM_END && err != Z_BUF_ERROR) {
      DCHECK_NE(err, Z_STREAM_ERROR);

      EnqueueBuffers(controller, std::move(buffers), exception_state);
      if (exception_state.HadException()) {
        return;
      }

      if (err == Z_DATA_ERROR) {
        exception_state.ThrowTypeError(
            String("The compressed data was not valid: ") + stream_.msg + ".");
      } else {
        exception_state.ThrowTypeError("The compressed data was not valid.");
      }
      return;
    }

    wtf_size_t bytes = out_buffer_.size() - stream_.avail_out;
    if (bytes) {
      buffers.push_back(
          DOMUint8Array::Create(base::span(out_buffer_).first(bytes)));
    }

    if (err == Z_STREAM_END) {
      reached_end_ = true;
      const bool junk_found = stream_.avail_in > 0;

      EnqueueBuffers(controller, std::move(buffers), exception_state);
      if (exception_state.HadException()) {
        return;
      }

      if (junk_found) {
        exception_state.ThrowTypeError(
            "Junk found after end of compressed data.");
      }
      return;
    }
  } while (stream_.avail_out == 0);

  DCHECK_EQ(stream_.avail_in, 0u);

  EnqueueBuffers(controller, std::move(buffers), exception_state);
}

void InflateTransformer::EnqueueBuffers(
    TransformStreamDefaultController* controller,
    HeapVector<Member<DOMUint8Array>, 1u> buffers,
    ExceptionState& exception_state) {
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

void InflateTransformer::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  TransformStreamTransformer::Trace(visitor);
}

}  // namespace blink
