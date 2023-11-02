// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EXPECT_BUG_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EXPECT_BUG_IMPL_H_

#include "net/test/gtest_util.h"

#define EXPECT_QUICHE_BUG_IMPL EXPECT_DFATAL
#define EXPECT_QUICHE_PEER_BUG_IMPL(statement, regex) statement;

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EXPECT_BUG_IMPL_H_
