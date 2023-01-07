// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// String manipulation functions used in the RLZ library.

#ifndef RLZ_LIB_STRING_UTILS_H_
#define RLZ_LIB_STRING_UTILS_H_

#include <string>

namespace rlz_lib {

bool IsAscii(unsigned char letter);

bool BytesToString(const unsigned char* data,
                   int data_len,
                   std::string* string);

bool GetHexValue(char letter, int* value);

int HexStringToInteger(const char* text);

}  // namespace rlz_lib

#endif  // RLZ_LIB_STRING_UTILS_H_
