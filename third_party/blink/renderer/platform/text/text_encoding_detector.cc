/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/text/text_encoding_detector.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/ced/src/compact_enc_det/compact_enc_det.h"

// third_party/ced/src/util/encodings/encodings.h, which is included
// by the include above, undefs UNICODE because that is a macro used
// internally in ced. If we later in the same translation unit do
// anything related to Windows or Windows headers those will then use
// the ASCII versions which we do not want. To avoid that happening in
// jumbo builds, we redefine UNICODE again here.
#if defined(OS_WIN)
#define UNICODE 1
#endif  // OS_WIN

namespace blink {

bool DetectTextEncoding(const char* data,
                        uint32_t length,
                        const char* hint_encoding_name,
                        const KURL& hint_url,
                        const char* hint_user_language,
                        WTF::TextEncoding* detected_encoding) {
  *detected_encoding = WTF::TextEncoding();
  // In general, do not use language hint. This helps get more
  // deterministic encoding detection results across devices. Note that local
  // file resources can still benefit from the hint.
  Language language = UNKNOWN_LANGUAGE;
  if (hint_url.Protocol() == "file")
    LanguageFromCode(hint_user_language, &language);
  int consumed_bytes;
  bool is_reliable;
  Encoding encoding = CompactEncDet::DetectEncoding(
      data, length, hint_url.GetString().Ascii().c_str(), nullptr, nullptr,
      EncodingNameAliasToEncoding(hint_encoding_name), language,
      CompactEncDet::WEB_CORPUS,
      false,  // Include 7-bit encodings to detect ISO-2022-JP
      &consumed_bytes, &is_reliable);

  if (encoding == UNKNOWN_ENCODING)
    *detected_encoding = WTF::UnknownEncoding();
  else
    *detected_encoding = WTF::TextEncoding(MimeEncodingName(encoding));

  // Should return false if the detected encoding is UTF8. This helps prevent
  // modern web sites from neglecting proper encoding labelling and simply
  // relying on browser-side encoding detection. Encoding detection is supposed
  // to work for web sites with legacy encoding only (so this doesn't have to
  // be applied to local file resources).
  // Detection failure leads |TextResourceDecoder| to use its default encoding
  // determined from system locale or TLD.
  return !(encoding == UNKNOWN_ENCODING ||
           (hint_url.Protocol() != "file" && encoding == UTF8));
}

}  // namespace blink
