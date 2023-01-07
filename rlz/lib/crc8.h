// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Crc8 utility functions.

#ifndef RLZ_LIB_CRC8_H_
#define RLZ_LIB_CRC8_H_

namespace rlz_lib {
// CRC-8 methods:
class Crc8 {
 public:
  static bool Generate(const unsigned char* data,
                       int length,
                       unsigned char* check_sum);
  static bool Verify(const unsigned char* data,
                     int length,
                     unsigned char checksum,
                     bool * matches);
};
}  // namespace rlz_lib

#endif  // RLZ_LIB_CRC8_H_
