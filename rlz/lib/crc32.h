// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A wrapper around ZLib's CRC function.

#ifndef RLZ_LIB_CRC32_H_
#define RLZ_LIB_CRC32_H_

namespace rlz_lib {

int Crc32(const unsigned char* buf, int length);
bool Crc32(const char* text, int* crc);

}  // namespace rlz_lib

#endif  // RLZ_LIB_CRC32_H_
