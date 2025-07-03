/*
 * Copyright (C) 2004, 2006, 2007, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_TEXT_CODEC_ICU_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_TEXT_CODEC_ICU_H_

#include <unicode/utypes.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

using UConverter = struct UConverter;

namespace blink {

class TextCodecIcu final : public TextCodec {
 public:
  static void RegisterEncodingNames(EncodingNameRegistrar);
  static void RegisterCodecs(TextCodecRegistrar);

  ~TextCodecIcu() override;

 private:
  explicit TextCodecIcu(const TextEncoding&);
  WTF_EXPORT static std::unique_ptr<TextCodec> Create(const TextEncoding&);

  String Decode(base::span<const uint8_t> data,
                FlushBehavior,
                bool stop_on_error,
                bool& saw_error) override;
  std::string Encode(base::span<const UChar>, UnencodableHandling) override;
  std::string Encode(base::span<const LChar>, UnencodableHandling) override;

  std::string EncodeCommon(base::span<const UChar>, UnencodableHandling);
  std::string EncodeInternal(base::span<const UChar>, UnencodableHandling);

  void CreateIcuConverter() const;
  void ReleaseIcuConverter() const;

  size_t DecodeToBuffer(base::span<UChar> target,
                        base::span<const char>& source,
                        bool flush,
                        UErrorCode&);

  TextEncoding encoding_;
  mutable UConverter* converter_icu_ = nullptr;
#if defined(USING_SYSTEM_ICU)
  mutable bool needs_gbk_fallbacks_ = false;
#endif

  FRIEND_TEST_ALL_PREFIXES(TextCodecIcuTest, IgnorableCodePoint);
};

struct IcuConverterWrapper {
  USING_FAST_MALLOC(IcuConverterWrapper);

 public:
  IcuConverterWrapper() : converter(nullptr) {}
  IcuConverterWrapper(const IcuConverterWrapper&) = delete;
  IcuConverterWrapper& operator=(const IcuConverterWrapper&) = delete;
  ~IcuConverterWrapper();

  UConverter* converter;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_TEXT_CODEC_ICU_H_
