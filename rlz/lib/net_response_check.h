// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef RLZ_LIB_NET_RESPONSE_CHECK_H_
#define RLZ_LIB_NET_RESPONSE_CHECK_H_

#include <stddef.h>

#include "rlz/lib/rlz_api.h"

// Checksum validation convenience call for RLZ network responses.
namespace rlz_lib {

// The maximum length of a ping response we will parse in bytes. If the response
// is bigger, please break it up into separate calls.
constexpr size_t kMaxPingResponseLength = 0x4000;  // 16K

// Checks if a ping response is valid - ie. it has a checksum line which
// is the CRC-32 checksum of the message up to the checksum. If
// checksum_idx is not NULL, it will get the index of the checksum, i.e. -
// the effective end of the message.
// Access: No restrictions.
bool RLZ_LIB_API IsPingResponseValid(const char* response, int* checksum_idx);

}  // namespace rlz_lib

#endif  // RLZ_LIB_NET_RESPONSE_CHECK_H_
