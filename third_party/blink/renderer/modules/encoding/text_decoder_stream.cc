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
#include "third_party/blink/renderer/core/streams/text_decoder_transformer.h"
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

TextDecoderStream* TextDecoderStream::Create(ScriptState* script_state,
                                             const String& label,
                                             const TextDecoderOptions* options,
                                             ExceptionState& exception_state) {
  TextEncoding encoding(label.StripWhiteSpace(&encoding::IsASCIIWhiteSpace));
  // The replacement encoding is not valid, but the Encoding API also
  // rejects aliases of the replacement encoding.
  if (!encoding.IsValid() ||
      EqualIgnoringAsciiCase(encoding.GetName(), "replacement")) {
    exception_state.ThrowRangeError(
        StrCat({"The encoding label provided ('", label, "') is invalid."}));
    return nullptr;
  }

  return MakeGarbageCollected<TextDecoderStream>(script_state, encoding,
                                                 options, exception_state);
}

TextDecoderStream::~TextDecoderStream() = default;

String TextDecoderStream::encoding() const {
  return encoding_.GetName().GetString().ToAsciiLower();
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
                                     const TextEncoding& encoding,
                                     const TextDecoderOptions* options,
                                     ExceptionState& exception_state)
    : transform_(TransformStream::Create(
          script_state,
          MakeGarbageCollected<TextDecoderTransformer>(script_state,
                                                       encoding,
                                                       options->fatal(),
                                                       options->ignoreBOM()),
          exception_state)),
      encoding_(encoding),
      fatal_(options->fatal()),
      ignore_bom_(options->ignoreBOM()) {}

}  // namespace blink
