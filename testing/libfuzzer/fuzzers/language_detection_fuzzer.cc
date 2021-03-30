// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/translate/core/language_detection/language_detection_util.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0) {
    return 0;
  }
  uint8_t ch = data[0];
  int lang_len = ch & 0xF;
  int html_lang_len = (ch >> 4) & 0xF;
  int text_len = static_cast<int>(size) - lang_len - html_lang_len;
  if ((text_len < 0) || (text_len % sizeof(char16_t) != 0)) {
    return 0;
  }
  std::string lang(reinterpret_cast<const char*>(data), lang_len);
  std::string html_lang(reinterpret_cast<const char*>(data + lang_len),
                        html_lang_len);
  std::u16string text(
      reinterpret_cast<const char16_t*>(data + lang_len + html_lang_len),
      text_len / sizeof(char16_t));
  std::string model_detected_language;
  bool is_model_reliable;
  float model_reliability_score;
  translate::DeterminePageLanguage(lang, html_lang, text,
                                   &model_detected_language, &is_model_reliable,
                                   model_reliability_score);
  return 0;
}
