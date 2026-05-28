// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TEXT_DECODER_TRANSFORMER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TEXT_DECODER_TRANSFORMER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

class TextCodec;

class CORE_EXPORT TextDecoderTransformer final
    : public TransformStreamTransformer {
 public:
  explicit TextDecoderTransformer(ScriptState* script_state,
                                  const TextEncoding& encoding,
                                  bool fatal,
                                  bool ignore_bom);
  ~TextDecoderTransformer() override;

  ScriptPromise<IDLUndefined> Transform(v8::Local<v8::Value> chunk,
                                        TransformStreamDefaultController*,
                                        ExceptionState&) override;

  ScriptPromise<IDLUndefined> Flush(TransformStreamDefaultController*,
                                    ExceptionState&) override;

  ScriptState* GetScriptState() override;

  void Trace(Visitor*) const override;

 private:
  std::unique_ptr<TextCodec> decoder_;
  Member<ScriptState> script_state_;
  const bool fatal_;
  const bool ignore_bom_;
  const bool encoding_has_bom_removal_;
  bool bom_seen_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TEXT_DECODER_TRANSFORMER_H_
