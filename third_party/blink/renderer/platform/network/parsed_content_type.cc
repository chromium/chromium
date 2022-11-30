/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#include "third_party/blink/renderer/platform/network/parsed_content_type.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/network/header_field_tokenizer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

// From http://tools.ietf.org/html/rfc2045#section-5.1:
//
// content := "Content-Type" ":" type "/" subtype
//            *(";" parameter)
//            ; Matching of media type and subtype
//            ; is ALWAYS case-insensitive.
//
// type := discrete-type / composite-type
//
// discrete-type := "text" / "image" / "audio" / "video" /
//                  "application" / extension-token
//
// composite-type := "message" / "multipart" / extension-token
//
// extension-token := ietf-token / x-token
//
// ietf-token := <An extension token defined by a
//                standards-track RFC and registered
//                with IANA.>
//
// x-token := <The two characters "X-" or "x-" followed, with
//             no intervening white space, by any token>
//
// subtype := extension-token / iana-token
//
// iana-token := <A publicly-defined extension token. Tokens
//                of this form must be registered with IANA
//                as specified in RFC 2048.>
//
// parameter := attribute "=" value
//
// attribute := token
//              ; Matching of attributes
//              ; is ALWAYS case-insensitive.
//
// value := token / quoted-string
//
// token := 1*<any (US-ASCII) CHAR except SPACE, CTLs,
//             or tspecials>
//
// tspecials :=  "(" / ")" / "<" / ">" / "@" /
//               "," / ";" / ":" / "\" / <">
//               "/" / "[" / "]" / "?" / "="
//               ; Must be in quoted-string,
//               ; to use within parameter values
ParsedContentType::ParsedContentType(const String& content_type, Mode mode) {
  HeaderFieldTokenizer tokenizer(content_type);

  StringView type, subtype;
  if (!tokenizer.ConsumeToken(Mode::kNormal, type)) {
    DVLOG(1) << "Failed to find `type' in '" << content_type << "'";
    return;
  }
  if (!tokenizer.Consume('/')) {
    DVLOG(1) << "Failed to find '/' in '" << content_type << "'";
    return;
  }
  if (!tokenizer.ConsumeToken(Mode::kNormal, subtype)) {
    DVLOG(1) << "Failed to find `subtype' in '" << content_type << "'";
    return;
  }

  StringBuilder builder;
  builder.Append(type);
  builder.Append('/');
  builder.Append(subtype);
  mime_type_ = builder.ToString();

  parameters_ =
      ParsedContentHeaderFieldParameters::Parse(std::move(tokenizer), mode);
}

String ParsedContentType::Charset() const {
  return ParameterValueForName("charset");
}

}  // namespace blink
