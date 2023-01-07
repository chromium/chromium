// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_TEST_FLAGS_UTILS_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_TEST_FLAGS_UTILS_H_

// When constructed, saves the current values of all QUIC flags. When
// destructed, restores all QUIC flags to the saved values.
class QuicFlagSaverImpl {
 public:
  QuicFlagSaverImpl();
  ~QuicFlagSaverImpl();

 private:
#define QUIC_FLAG(flag, value) bool saved_##flag##_;
#include "net/third_party/quiche/src/quiche/quic/core/quic_flags_list.h"
#undef QUIC_FLAG
#define QUIC_PROTOCOL_FLAG(type, flag, ...) type saved_##flag##_;
#include "net/third_party/quiche/src/quiche/quic/core/quic_protocol_flags_list.h"
#undef QUIC_PROTOCOL_FLAG
};

//// Checks if all QUIC flags are on their default values on construction.
class QuicFlagChecker {
 public:
  QuicFlagChecker();
};

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_TEST_FLAGS_UTILS_H_
