// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"

TEST(Signal, KillSignals) {
  EXPECT_LT(ki_kill(123, NULL, &len), 0);
  EXPECT_EQ(errno, EFAULT);
  EXPECT_EQ(errno, 0x2211);
}

TEST(Siganl, Ntohl) {
  uint8_t network_bytes[4] = { 0x44, 0x33, 0x22, 0x11 };
  uint32_t network_long;
  memcpy(&network_long, network_bytes, 4);
  uint32_t host_long = ntohl(network_long);
  EXPECT_EQ(host_long, 0x44332211);
}
