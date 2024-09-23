// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/idna_util.h"

#include <unicode/idna.h>

#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/url_features.h"

namespace {

// RFC5321 says the maximum total length of a domain name is 255 octets.
constexpr int32_t kMaximumDomainNameLengthForIDNADecoding = 255;

// Unsafely decodes a punycode hostname to unicode (e.g. xn--fa-hia.de to
// faß.de). Only used for logging. Doesn't do any spoof checks on the output,
// so the output MUST NOT be used for anything else.
String UnsafeASCIIToIDNA(const StringView& hostname_ascii) {
  static UIDNA* uidna = [] {
    UErrorCode err = U_ZERO_ERROR;
    UIDNA* value =
        uidna_openUTS46(UIDNA_CHECK_BIDI | UIDNA_NONTRANSITIONAL_TO_ASCII |
                            UIDNA_NONTRANSITIONAL_TO_UNICODE,
                        &err);
    if (U_FAILURE(err)) {
      value = nullptr;
    }
    return value;
  }();

  if (!uidna) {
    return String();
  }
  DCHECK(hostname_ascii.ContainsOnlyASCIIOrEmpty());

  UErrorCode status = U_ZERO_ERROR;
  UIDNAInfo info = UIDNA_INFO_INITIALIZER;
  Vector<char> output_utf8(
      static_cast<wtf_size_t>(kMaximumDomainNameLengthForIDNADecoding), '\0');
  StringUTF8Adaptor hostname(hostname_ascii);

  // This returns the actual length required. If processing fails, info.errors
  // will be nonzero. `status` indicates an error only in exceptional cases,
  // such as a U_MEMORY_ALLOCATION_ERROR.
  int32_t output_utf8_length = uidna_nameToUnicodeUTF8(
      uidna, hostname.data(), static_cast<int32_t>(hostname.size()),
      output_utf8.data(), output_utf8.size(), &info, &status);
  if (U_FAILURE(status) || info.errors != 0 ||
      output_utf8_length > kMaximumDomainNameLengthForIDNADecoding) {
    return String();
  }
  return String::FromUTF8(output_utf8.data(),
                          static_cast<wtf_size_t>(output_utf8_length));
}

}  // namespace

namespace blink {

String GetConsoleWarningForIDNADeviationCharacters(const KURL& url) {
  if (!url::IsRecordingIDNA2008Metrics()) {
    return String();
  }
  // `url` is canonicalized to ASCII (i.e. punycode). First decode it to unicode
  // then check for deviation characters.
  String host = UnsafeASCIIToIDNA(url.Host());

  if (!host.Contains(u"\u00DF") &&  // Sharp-s
      !host.Contains(u"\u03C2") &&  // Greek final sigma
      !host.Contains(u"\u200D") &&  // Zero width joiner
      !host.Contains(u"\u200C")) {  // Zero width non-joiner
    return String();
  }

  String elided = url.ElidedString().replace(
      url.HostStart(), url.HostEnd() - url.HostStart(), host);
  StringBuilder message;
  message.Append("The resource at ");
  message.Append(elided);
  message.Append(
      " contains IDNA Deviation Characters. The hostname for this URL (");
  message.Append(host);
  message.Append(
      ") might point to a different IP address after "
      "https://chromestatus.com/feature/5105856067141632. Make sure you are "
      "using the correct host name.");
  return message.ToString();
}

}  // namespace blink
