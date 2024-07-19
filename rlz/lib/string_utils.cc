// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// String manipulation functions used in the RLZ library.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "rlz/lib/string_utils.h"

#include "base/strings/string_number_conversions.h"
#include "rlz/lib/assert.h"

namespace rlz_lib {

bool IsAscii(unsigned char letter) {
  return letter < 0x80;
}

bool GetHexValue(char letter, int* value) {
  if (!value) {
    ASSERT_STRING("GetHexValue: Invalid output paramter");
    return false;
  }
  *value = 0;

  if (letter >= '0' && letter <= '9')
    *value = letter - '0';
  else if (letter >= 'a' && letter <= 'f')
    *value = (letter - 'a') + 0xA;
  else if (letter >= 'A' && letter <= 'F')
    *value = (letter - 'A') + 0xA;
  else
    return false;

  return true;
}

int HexStringToInteger(const char* text) {
  if (!text) {
    ASSERT_STRING("HexStringToInteger: text is NULL.");
    return 0;
  }

  int idx = 0;
  // Ignore leading whitespace.
  while (text[idx] == '\t' || text[idx] == ' ')
    idx++;

  if ((text[idx] == '0') &&
      (text[idx + 1] == 'X' || text[idx + 1] == 'x'))
    idx +=2;  // String is of the form 0x...

  int number = 0;
  int digit = 0;
  for (; text[idx] != '\0'; idx++) {
    if (!GetHexValue(text[idx], &digit)) {
      // Ignore trailing whitespaces, but assert on other trailing characters.
      bool only_whitespaces = true;
      while (only_whitespaces && text[idx])
        only_whitespaces = (text[idx++] == ' ');
      if (!only_whitespaces)
        ASSERT_STRING("HexStringToInteger: text contains non-hex characters.");
      return number;
    }
    number = (number << 4) | digit;
  }

  return number;
}

bool BytesToString(const unsigned char* data,
                   int data_len,
                   std::string* string) {
  if (!string)
    return false;

  string->clear();
  if (data_len < 1 || !data)
    return false;

  *string = base::HexEncode(data, static_cast<size_t>(data_len));
  return true;
}

}  // namespace rlz_lib
