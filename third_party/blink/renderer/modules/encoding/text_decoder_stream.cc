// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encoding/text_decoder_stream.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/streams/retain_wrapper_during_construction.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/encoding/encoding.h"
#include "third_party/blink/renderer/modules/encoding/text_decoder_options.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/wtf/string_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {

class TextDecoderStream::Transformer final : public TransformStreamTransformer {
 public:
  explicit Transformer(ScriptState* script_state,
                       WTF::TextEncoding encoding,
                       bool fatal,
                       bool ignore_bom)
      : decoder_(NewTextCodec(encoding)),
        script_state_(script_state),
        fatal_(fatal),
        ignore_bom_(ignore_bom),
        encoding_has_bom_removal_(EncodingHasBomRemoval(encoding)) {}

  // Implements the type conversion part of the "decode and enqueue a chunk"
  // algorithm.
  void Transform(v8::Local<v8::Value> chunk,
                 TransformStreamDefaultController* controller,
                 ExceptionState& exception_state) override {
    ArrayBufferOrArrayBufferView bufferSource;
    V8ArrayBufferOrArrayBufferView::ToImpl(
        script_state_->GetIsolate(), chunk, bufferSource,
        UnionTypeConversionMode::kNotNullable, exception_state);
    if (exception_state.HadException())
      return;

    // This implements the "get a copy of the bytes held by the buffer source"
    // algorithm (https://heycam.github.io/webidl/#dfn-get-buffer-source-copy).
    if (bufferSource.IsArrayBufferView()) {
      const auto* view = bufferSource.GetAsArrayBufferView().View();
      // If IsDetachedBuffer(O), then throw a TypeError.
      if (view->buffer()->IsNeutered()) {
        exception_state.ThrowTypeError(
            ExceptionMessages::FailedToConvertJSValue("BufferSource"));
        return;
      }
      const char* start = static_cast<const char*>(view->BaseAddress());
      uint32_t length = view->byteLength();
      DecodeAndEnqueue(start, length, WTF::FlushBehavior::kDoNotFlush,
                       controller, exception_state);
      return;
    }
    DCHECK(bufferSource.IsArrayBuffer());
    const auto* array_buffer = bufferSource.GetAsArrayBuffer();
    // If IsDetachedBuffer(O), then throw a TypeError.
    if (array_buffer->IsNeutered()) {
      exception_state.ThrowTypeError(
          ExceptionMessages::FailedToConvertJSValue("BufferSource"));
      return;
    }
    const char* start = static_cast<const char*>(array_buffer->Data());
    uint32_t length = array_buffer->ByteLength();
    DecodeAndEnqueue(start, length, WTF::FlushBehavior::kDoNotFlush, controller,
                     exception_state);
  }

  // Implements the "encode and flush" algorithm.
  void Flush(TransformStreamDefaultController* controller,
             ExceptionState& exception_state) override {
    DecodeAndEnqueue(nullptr, 0u, WTF::FlushBehavior::kDataEOF, controller,
                     exception_state);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(script_state_);
    TransformStreamTransformer::Trace(visitor);
  }

 private:
  // Implements the second part of "decode and enqueue a chunk" as well as the
  // "flush and enqueue" algorithm.
  void DecodeAndEnqueue(const char* start,
                        uint32_t length,
                        WTF::FlushBehavior flush,
                        TransformStreamDefaultController* controller,
                        ExceptionState& exception_state) {
    const UChar kBOM = 0xFEFF;

    bool saw_error = false;
    String outputChunk =
        decoder_->Decode(start, length, flush, fatal_, saw_error);

    if (fatal_ && saw_error) {
      exception_state.ThrowTypeError("The encoded data was not valid.");
      return;
    }

    if (outputChunk.IsEmpty())
      return;

    if (!ignore_bom_ && !bom_seen_) {
      bom_seen_ = true;
      if (encoding_has_bom_removal_ && outputChunk[0] == kBOM) {
        outputChunk.Remove(0);
        if (outputChunk.IsEmpty())
          return;
      }
    }

    controller->Enqueue(ToV8(outputChunk, script_state_), exception_state);
  }

  static bool EncodingHasBomRemoval(const WTF::TextEncoding& encoding) {
    String name(encoding.GetName());
    return name == "UTF-8" || name == "UTF-16LE" || name == "UTF-16BE";
  }

  std::unique_ptr<WTF::TextCodec> decoder_;
  // There is no danger of ScriptState leaking across worlds because a
  // TextDecoderStream can only be accessed from the world that created it.
  Member<ScriptState> script_state_;
  const bool fatal_;
  const bool ignore_bom_;
  const bool encoding_has_bom_removal_;
  bool bom_seen_;

  DISALLOW_COPY_AND_ASSIGN(Transformer);
};

TextDecoderStream* TextDecoderStream::Create(ScriptState* script_state,
                                             const String& label,
                                             const TextDecoderOptions& options,
                                             ExceptionState& exception_state) {
  WTF::TextEncoding encoding(
      label.StripWhiteSpace(&encoding::IsASCIIWhiteSpace));
  // The replacement encoding is not valid, but the Encoding API also
  // rejects aliases of the replacement encoding.
  if (!encoding.IsValid() ||
      strcasecmp(encoding.GetName(), "replacement") == 0) {
    exception_state.ThrowRangeError("The encoding label provided ('" + label +
                                    "') is invalid.");
    return nullptr;
  }

  return new TextDecoderStream(script_state, encoding, options,
                               exception_state);
}

TextDecoderStream::~TextDecoderStream() = default;

String TextDecoderStream::encoding() const {
  return String(encoding_.GetName()).LowerASCII();
}

ScriptValue TextDecoderStream::readable(ScriptState* script_state,
                                        ExceptionState& exception_state) const {
  return transform_->Readable(script_state, exception_state);
}

ScriptValue TextDecoderStream::writable(ScriptState* script_state,
                                        ExceptionState& exception_state) const {
  return transform_->Writable(script_state, exception_state);
}

void TextDecoderStream::Trace(Visitor* visitor) {
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
}

TextDecoderStream::TextDecoderStream(ScriptState* script_state,
                                     const WTF::TextEncoding& encoding,
                                     const TextDecoderOptions& options,
                                     ExceptionState& exception_state)
    : transform_(new TransformStream()),
      encoding_(encoding),
      fatal_(options.fatal()),
      ignore_bom_(options.ignoreBOM()) {
  if (!RetainWrapperDuringConstruction(this, script_state)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot queue task to retain wrapper");
    return;
  }
  transform_->Init(new Transformer(script_state, encoding, fatal_, ignore_bom_),
                   script_state, exception_state);
}

}  // namespace blink
