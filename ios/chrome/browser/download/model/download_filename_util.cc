// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/model/download_filename_util.h"

#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/icubridge/normalizer.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace download_model {

std::string NormalizeFileName(std::string_view file_name) {
  if (file_name.empty()) {
    return std::string();
  }
  // If normalization is unavailable the bridge returns the input unchanged,
  // in which case the steps below still run on the un-decomposed text.
  const std::u16string decomposed =
      base::i18n::IcuBridge::GetInstance().normalizer().Normalize(
          base::i18n::IcuBridge::Normalizer::NormalizationForm::NFD,
          base::UTF8ToUTF16(file_name));
  icu::UnicodeString u16(decomposed.data(),
                         base::checked_cast<int32_t>(decomposed.length()));
  // Strip Non-Spacing Marks (Mn) in place.
  icu::UnicodeString stripped;
  for (int32_t i = 0; i < u16.length();) {
    UChar32 cp = u16.char32At(i);
    int32_t cp_len = U16_LENGTH(cp);
    if (u_charType(cp) != U_NON_SPACING_MARK) {
      stripped.append(cp);
    }
    i += cp_len;
  }
  // Case-fold in place via ICU directly to avoid extra UTF-16/UTF-8
  // round-trips through `base::i18n::FoldCase`.
  stripped.foldCase();
  std::string utf8;
  stripped.toUTF8String(utf8);
  return utf8;
}

}  // namespace download_model
