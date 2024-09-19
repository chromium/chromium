// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encoding/text_encoder_stream.h"

#include <stdint.h>
#include <string.h>

#include <memory>
#include <optional>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"
#include "v8/include/v8.h"

namespace blink {

class TextEncoderStream::Transformer final : public TransformStreamTransformer {
 public:
  explicit Transformer(ScriptState* script_state)
      : encoder_(NewTextCodec(WTF::UTF8Encoding())),
        script_state_(script_state) {}

  Transformer(const Transformer&) = delete;
  Transformer& operator=(const Transformer&) = delete;

  // Implements the "encode and enqueue a chunk" algorithm. For efficiency, only
  // the characters at the end of chunks are special-cased.
  ScriptPromise<IDLUndefined> Transform(
      v8::Local<v8::Value> chunk,
      TransformStreamDefaultController* controller,
      ExceptionState& exception_state) override {
    V8StringResource<> input_resource{script_state_->GetIsolate(), chunk};
    if (!input_resource.Prepare(exception_state)) {
      return EmptyPromise();
    }
    const String input = input_resource;
    if (input.empty())
      return ToResolvedUndefinedPromise(script_state_.Get());

    const std::optional<UChar> high_surrogate = pending_high_surrogate_;
    pending_high_surrogate_ = std::nullopt;
    std::string prefix;
    std::string result;
    if (input.Is8Bit()) {
      if (high_surrogate.has_value()) {
        // An 8-bit code unit can never be part of an astral character, so no
        // check is needed.
        prefix = ReplacementCharacterInUtf8();
      }
      result = encoder_->Encode(input.Span8(), WTF::kNoUnencodables);
    } else {
      bool have_output =
          Encode16BitString(input, high_surrogate, &prefix, &result);
      if (!have_output)
        return ToResolvedUndefinedPromise(script_state_.Get());
    }

    DOMUint8Array* array =
        CreateDOMUint8ArrayFromTwoStdStringsConcatenated(prefix, result);
    controller->enqueue(script_state_, ScriptValue::From(script_state_, array),
                        exception_state);

    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  // Implements the "encode and flush" algorithm.
  ScriptPromise<IDLUndefined> Flush(
      TransformStreamDefaultController* controller,
      ExceptionState& exception_state) override {
    if (!pending_high_surrogate_.has_value())
      return ToResolvedUndefinedPromise(script_state_.Get());

    const std::string replacement_character = ReplacementCharacterInUtf8();
    controller->enqueue(
        script_state_,
        ScriptValue::From(
            script_state_,
            DOMUint8Array::Create(base::as_byte_span(replacement_character))),
        exception_state);

    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  ScriptState* GetScriptState() override { return script_state_.Get(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    TransformStreamTransformer::Trace(visitor);
  }

 private:
  static std::string ReplacementCharacterInUtf8() { return "\ufffd"; }

  static DOMUint8Array* CreateDOMUint8ArrayFromTwoStdStringsConcatenated(
      const std::string& string1,
      const std::string& string2) {
    const wtf_size_t length1 = static_cast<wtf_size_t>(string1.length());
    const wtf_size_t length2 = static_cast<wtf_size_t>(string2.length());
    DOMUint8Array* const array = DOMUint8Array::Create(length1 + length2);
    auto [string1_span, string2_span] = array->ByteSpan().split_at(length1);
    string1_span.copy_from(base::as_byte_span(string1));
    string2_span.copy_from(base::as_byte_span(string2));
    return array;
  }

  // Returns true if either |*prefix| or |*result| have been set to a non-empty
  // value.
  bool Encode16BitString(const String& input,
                         std::optional<UChar> high_surrogate,
                         std::string* prefix,
                         std::string* result) {
    base::span<const UChar> input_span = input.Span16();
    DCHECK(!input_span.empty());
    if (high_surrogate.has_value()) {
      const UChar code_unit = input_span.front();
      if (code_unit >= 0xDC00 && code_unit <= 0xDFFF) {
        const UChar astral_character[2] = {high_surrogate.value(), code_unit};
        // Third argument is ignored, as above.
        *prefix = encoder_->Encode(base::span(astral_character),
                                   WTF::kNoUnencodables);
        input_span = input_span.subspan<1u>();
        if (input_span.empty()) {
          return true;
        }
      } else {
        *prefix = ReplacementCharacterInUtf8();
      }
    }

    const UChar final_token = input_span.back();
    if (final_token >= 0xD800 && final_token <= 0xDBFF) {
      pending_high_surrogate_ = final_token;
      input_span = input_span.first(input_span.size() - 1u);
      if (input_span.empty()) {
        return prefix->length() != 0;
      }
    }

    // Third argument is ignored, as above.
    *result = encoder_->Encode(input_span, WTF::kEntitiesForUnencodables);
    DCHECK_NE(result->length(), 0u);
    return true;
  }

  std::unique_ptr<WTF::TextCodec> encoder_;
  // There is no danger of ScriptState leaking across worlds because a
  // TextEncoderStream can only be accessed from the world that created it.
  Member<ScriptState> script_state_;
  std::optional<UChar> pending_high_surrogate_;
};

TextEncoderStream* TextEncoderStream::Create(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  return MakeGarbageCollected<TextEncoderStream>(script_state, exception_state);
}

TextEncoderStream::~TextEncoderStream() = default;

String TextEncoderStream::encoding() const {
  return "utf-8";
}

ReadableStream* TextEncoderStream::readable() const {
  return transform_->Readable();
}

WritableStream* TextEncoderStream::writable() const {
  return transform_->Writable();
}

void TextEncoderStream::Trace(Visitor* visitor) const {
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
}

TextEncoderStream::TextEncoderStream(ScriptState* script_state,
                                     ExceptionState& exception_state)
    : transform_(TransformStream::Create(
          script_state,
          MakeGarbageCollected<Transformer>(script_state),
          exception_state)) {}

}  // namespace blink
