// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/text_decoder_transformer.h"

#include <limits>
#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding_registry.h"

namespace blink {

namespace {

bool EncodingHasBomRemoval(const TextEncoding& encoding) {
  const AtomicString& name = encoding.GetName();
  return name == "UTF-8" || name == "UTF-16LE" || name == "UTF-16BE";
}

}  // namespace

TextDecoderTransformer::TextDecoderTransformer(ScriptState* script_state,
                                               const TextEncoding& encoding,
                                               bool fatal,
                                               bool ignore_bom)
    : decoder_(NewTextCodec(encoding)),
      script_state_(script_state),
      fatal_(fatal),
      ignore_bom_(ignore_bom),
      encoding_has_bom_removal_(EncodingHasBomRemoval(encoding)) {
  DCHECK(decoder_);
}

TextDecoderTransformer::~TextDecoderTransformer() = default;

ScriptPromise<IDLUndefined> TextDecoderTransformer::Transform(
    v8::Local<v8::Value> chunk,
    TransformStreamDefaultController* controller,
    ExceptionState& exception_state) {
  auto* buffer_source = V8BufferSource::Create(script_state_->GetIsolate(),
                                               chunk, exception_state);
  if (exception_state.HadException()) {
    return EmptyPromise();
  }

  DOMArrayPiece array_piece(buffer_source);
  if (array_piece.ByteLength() > std::numeric_limits<uint32_t>::max()) {
    exception_state.ThrowRangeError(
        "Buffer size exceeds maximum heap object size.");
    return EmptyPromise();
  }

  bool saw_error = false;
  String output_chunk = decoder_->Decode(
      array_piece.ByteSpan(), FlushBehavior::kDoNotFlush, fatal_, saw_error);
  if (fatal_ && saw_error) {
    exception_state.ThrowTypeError("The encoded data is not valid.");
    return EmptyPromise();
  }

  if (output_chunk.empty()) {
    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  if (!ignore_bom_ && !bom_seen_) {
    bom_seen_ = true;
    const UChar kBOM = 0xFEFF;
    if (encoding_has_bom_removal_ && output_chunk[0] == kBOM) {
      output_chunk.erase(0, 1);
      if (output_chunk.empty()) {
        return ToResolvedUndefinedPromise(script_state_.Get());
      }
    }
  }

  controller->enqueue(
      script_state_,
      ScriptValue(script_state_->GetIsolate(),
                  V8String(script_state_->GetIsolate(), output_chunk)),
      exception_state);

  return ToResolvedUndefinedPromise(script_state_.Get());
}

ScriptPromise<IDLUndefined> TextDecoderTransformer::Flush(
    TransformStreamDefaultController* controller,
    ExceptionState& exception_state) {
  bool saw_error = false;
  String output_chunk =
      decoder_->Decode({}, FlushBehavior::kDataEof, fatal_, saw_error);
  if (fatal_ && saw_error) {
    exception_state.ThrowTypeError("The encoded data is not valid.");
    return EmptyPromise();
  }

  if (output_chunk.empty()) {
    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  if (!ignore_bom_ && !bom_seen_) {
    bom_seen_ = true;
    const UChar kBOM = 0xFEFF;
    if (encoding_has_bom_removal_ && output_chunk[0] == kBOM) {
      output_chunk.erase(0, 1);
      if (output_chunk.empty()) {
        return ToResolvedUndefinedPromise(script_state_.Get());
      }
    }
  }

  controller->enqueue(
      script_state_,
      ScriptValue(script_state_->GetIsolate(),
                  V8String(script_state_->GetIsolate(), output_chunk)),
      exception_state);

  return ToResolvedUndefinedPromise(script_state_.Get());
}

ScriptState* TextDecoderTransformer::GetScriptState() {
  return script_state_.Get();
}

void TextDecoderTransformer::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  TransformStreamTransformer::Trace(visitor);
}

}  // namespace blink
