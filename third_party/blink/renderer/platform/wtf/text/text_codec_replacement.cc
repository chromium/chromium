// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec_replacement.h"

#include <memory>
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace WTF {

TextCodecReplacement::TextCodecReplacement()
    : replacement_error_returned_(false) {}

void TextCodecReplacement::RegisterEncodingNames(
    EncodingNameRegistrar registrar) {
  // Taken from the alias table at·https://encoding.spec.whatwg.org/
  registrar("replacement", "replacement");
  registrar("csiso2022kr", "replacement");
  registrar("hz-gb-2312", "replacement");
  registrar("iso-2022-cn", "replacement");
  registrar("iso-2022-cn-ext", "replacement");
  registrar("iso-2022-kr", "replacement");
}

static std::unique_ptr<TextCodec> NewStreamingTextDecoderReplacement(
    const TextEncoding&,
    const void*) {
  return std::make_unique<TextCodecReplacement>();
}

void TextCodecReplacement::RegisterCodecs(TextCodecRegistrar registrar) {
  registrar("replacement", NewStreamingTextDecoderReplacement, nullptr);
}

String TextCodecReplacement::Decode(base::span<const uint8_t> data,
                                    FlushBehavior,
                                    bool,
                                    bool& saw_error) {
  // https://encoding.spec.whatwg.org/#replacement-decoder

  // 1. If byte is end-of-stream, return finished.
  if (data.empty()) {
    return String();
  }

  // 2. If replacement error returned flag is unset, set the replacement
  // error returned flag and return error.
  if (!replacement_error_returned_) {
    replacement_error_returned_ = true;
    saw_error = true;
    return String(&kReplacementCharacter, 1u);
  }

  // 3. Return finished.
  return String();
}

}  // namespace WTF
