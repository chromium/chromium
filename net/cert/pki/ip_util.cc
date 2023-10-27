// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/pki/ip_util.h"

namespace net {

bool IsValidNetmask(der::Input mask) {
  if (mask.Length() != kIPv4AddressSize && mask.Length() != kIPv6AddressSize) {
    return false;
  }

  for (size_t i = 0; i < mask.Length(); i++) {
    uint8_t b = mask[i];
    if (b != 0xff) {
      // b must be all ones followed by all zeros, so ~b must be all zeros
      // followed by all ones.
      uint8_t inv = ~b;
      if ((inv & (inv + 1)) != 0) {
        return false;
      }
      // The remaining bytes must be all zeros.
      for (size_t j = i + 1; j < mask.Length(); j++) {
        if (mask[j] != 0) {
          return false;
        }
      }
      return true;
    }
  }

  return true;
}

bool IPAddressMatchesWithNetmask(der::Input addr1,
                                 der::Input addr2,
                                 der::Input mask) {
  if (addr1.Length() != addr2.Length() || addr1.Length() != mask.Length()) {
    return false;
  }
  for (size_t i = 0; i < addr1.Length(); i++) {
    if ((addr1[i] & mask[i]) != (addr2[i] & mask[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace net
