// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_EXPECT_BUG_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_EXPECT_BUG_IMPL_H_

#include "net/test/gtest_util.h"

#define EXPECT_QUIC_BUG_IMPL EXPECT_DFATAL
#define EXPECT_QUIC_PEER_BUG_IMPL(statement, regex) statement;

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_EXPECT_BUG_IMPL_H_
