// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/text_codec_replacement.h"

#include <memory>

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

TextCodecReplacement::TextCodecReplacement()
    : replacement_error_returned_(false) {}

void TextCodecReplacement::RegisterEncodingNames(
    EncodingNameRegistrar registrar) {
  AtomicString canonical_name("replacement");
  // Taken from the alias table at·https://encoding.spec.whatwg.org/
  registrar("replacement", canonical_name);
  registrar("csiso2022kr", canonical_name);
  registrar("hz-gb-2312", canonical_name);
  registrar("iso-2022-cn", canonical_name);
  registrar("iso-2022-cn-ext", canonical_name);
  registrar("iso-2022-kr", canonical_name);
}

static std::unique_ptr<TextCodec> NewStreamingTextDecoderReplacement(
    const TextEncoding&) {
  return std::make_unique<TextCodecReplacement>();
}

void TextCodecReplacement::RegisterCodecs(TextCodecRegistrar registrar) {
  registrar("replacement", NewStreamingTextDecoderReplacement);
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
    return String(base::span_from_ref(uchar::kReplacementCharacter));
  }

  // 3. Return finished.
  return String();
}

}  // namespace blink
