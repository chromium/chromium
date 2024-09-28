// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_string_util.h"

#include <string_view>

#include "base/i18n/case_conversion.h"
#include "base/i18n/i18n_constants.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/ucnv.h"

namespace net {

const char* const kCharsetLatin1 = base::kCodepageLatin1;

bool ConvertToUtf8(std::string_view text,
                   const char* charset,
                   std::string* output) {
  output->clear();

  UErrorCode err = U_ZERO_ERROR;
  UConverter* converter(ucnv_open(charset, &err));
  if (U_FAILURE(err))
    return false;

  // A single byte in a legacy encoding can be expanded to 3 bytes in UTF-8.
  // A 'two-byte character' in a legacy encoding can be expanded to 4 bytes
  // in UTF-8. Therefore, the expansion ratio is 3 at most.
  output->resize(text.length() * 3);
  size_t output_length =
      ucnv_toAlgorithmic(UCNV_UTF8, converter, output->data(), output->length(),
                         text.data(), text.length(), &err);
  ucnv_close(converter);
  if (U_FAILURE(err)) {
    output->clear();
    return false;
  }

  output->resize(output_length);
  return true;
}

bool ConvertToUtf8AndNormalize(std::string_view text,
                               const char* charset,
                               std::string* output) {
  return base::ConvertToUtf8AndNormalize(text, charset, output);
}

bool ConvertToUTF16(std::string_view text,
                    const char* charset,
                    std::u16string* output) {
  return base::CodepageToUTF16(text, charset,
                               base::OnStringConversionError::FAIL, output);
}

bool ConvertToUTF16WithSubstitutions(std::string_view text,
                                     const char* charset,
                                     std::u16string* output) {
  return base::CodepageToUTF16(
      text, charset, base::OnStringConversionError::SUBSTITUTE, output);
}

bool ToUpperUsingLocale(std::u16string_view str, std::u16string* output) {
  *output = base::i18n::ToUpper(str);
  return true;
}

}  // namespace net
