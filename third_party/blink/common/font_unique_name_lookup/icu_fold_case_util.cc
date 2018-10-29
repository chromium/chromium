// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"
#include "third_party/icu/source/common/unicode/unistr.h"

namespace blink {

std::string IcuFoldCase(const std::string& name_request) {
  icu::UnicodeString name_request_unicode =
      icu::UnicodeString::fromUTF8(name_request);
  name_request_unicode.foldCase();
  std::string name_request_lower;
  name_request_unicode.toUTF8String(name_request_lower);
  return name_request_lower;
}

}  // namespace blink
