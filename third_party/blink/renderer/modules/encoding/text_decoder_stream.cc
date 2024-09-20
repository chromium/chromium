// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encoding/text_decoder_stream.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_text_decoder_options.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/encoding/encoding.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
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

  Transformer(const Transformer&) = delete;
  Transformer& operator=(const Transformer&) = delete;

  // Implements the type conversion part of the "decode and enqueue a chunk"
  // algorithm.
  ScriptPromise<IDLUndefined> Transform(
      v8::Local<v8::Value> chunk,
      TransformStreamDefaultController* controller,
      ExceptionState& exception_state) override {
    auto* buffer_source = V8BufferSource::Create(script_state_->GetIsolate(),
                                                 chunk, exception_state);
    if (exception_state.HadException())
      return EmptyPromise();

    // This implements the "get a copy of the bytes held by the buffer source"
    // algorithm (https://webidl.spec.whatwg.org/#dfn-get-buffer-source-copy).
    DOMArrayPiece array_piece(buffer_source);
    if (array_piece.ByteLength() > std::numeric_limits<uint32_t>::max()) {
      exception_state.ThrowRangeError(
          "Buffer size exceeds maximum heap object size.");
      return EmptyPromise();
    }
    DecodeAndEnqueue(array_piece.ByteSpan(), WTF::FlushBehavior::kDoNotFlush,
                     controller, exception_state);
    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  // Implements the "encode and flush" algorithm.
  ScriptPromise<IDLUndefined> Flush(
      TransformStreamDefaultController* controller,
      ExceptionState& exception_state) override {
    DecodeAndEnqueue({}, WTF::FlushBehavior::kDataEOF, controller,
                     exception_state);

    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  ScriptState* GetScriptState() override { return script_state_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    TransformStreamTransformer::Trace(visitor);
  }

 private:
  // Implements the second part of "decode and enqueue a chunk" as well as the
  // "flush and enqueue" algorithm.
  void DecodeAndEnqueue(base::span<const uint8_t> data,
                        WTF::FlushBehavior flush,
                        TransformStreamDefaultController* controller,
                        ExceptionState& exception_state) {
    const UChar kBOM = 0xFEFF;

    bool saw_error = false;
    String output_chunk = decoder_->Decode(data, flush, fatal_, saw_error);

    if (fatal_ && saw_error) {
      exception_state.ThrowTypeError("The encoded data was not valid.");
      return;
    }

    if (output_chunk.empty()) {
      return;
    }

    if (!ignore_bom_ && !bom_seen_) {
      bom_seen_ = true;
      if (encoding_has_bom_removal_ && output_chunk[0] == kBOM) {
        output_chunk.Remove(0);
        if (output_chunk.empty()) {
          return;
        }
      }
    }

    controller->enqueue(
        script_state_,
        ScriptValue(script_state_->GetIsolate(),
                    V8String(script_state_->GetIsolate(), output_chunk)),
        exception_state);
  }

  static bool EncodingHasBomRemoval(const WTF::TextEncoding& encoding) {
    const AtomicString& name = encoding.GetName();
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
};

TextDecoderStream* TextDecoderStream::Create(ScriptState* script_state,
                                             const String& label,
                                             const TextDecoderOptions* options,
                                             ExceptionState& exception_state) {
  WTF::TextEncoding encoding(
      label.StripWhiteSpace(&encoding::IsASCIIWhiteSpace));
  // The replacement encoding is not valid, but the Encoding API also
  // rejects aliases of the replacement encoding.
  if (!encoding.IsValid() ||
      WTF::EqualIgnoringASCIICase(encoding.GetName(), "replacement")) {
    exception_state.ThrowRangeError("The encoding label provided ('" + label +
                                    "') is invalid.");
    return nullptr;
  }

  return MakeGarbageCollected<TextDecoderStream>(script_state, encoding,
                                                 options, exception_state);
}

TextDecoderStream::~TextDecoderStream() = default;

String TextDecoderStream::encoding() const {
  return encoding_.GetName().GetString().LowerASCII();
}

ReadableStream* TextDecoderStream::readable() const {
  return transform_->Readable();
}

WritableStream* TextDecoderStream::writable() const {
  return transform_->Writable();
}

void TextDecoderStream::Trace(Visitor* visitor) const {
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
}

TextDecoderStream::TextDecoderStream(ScriptState* script_state,
                                     const WTF::TextEncoding& encoding,
                                     const TextDecoderOptions* options,
                                     ExceptionState& exception_state)
    : transform_(TransformStream::Create(
          script_state,
          MakeGarbageCollected<Transformer>(script_state,
                                            encoding,
                                            options->fatal(),
                                            options->ignoreBOM()),
          exception_state)),
      encoding_(encoding),
      fatal_(options->fatal()),
      ignore_bom_(options->ignoreBOM()) {}

}  // namespace blink
