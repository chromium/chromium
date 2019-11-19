// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_MOCK_LOG_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_MOCK_LOG_IMPL_H_

#include "base/test/mock_log.h"
#include "testing/gmock/include/gmock/gmock.h"  // IWYU pragma: export

using QuicMockLogImpl = base::test::MockLog;
#define CREATE_QUIC_MOCK_LOG_IMPL(log) QuicMockLog log

#define EXPECT_QUIC_LOG_CALL_IMPL(log) EXPECT_CALL(log, Log(_, _, _, _, _))

#define EXPECT_QUIC_LOG_CALL_CONTAINS_IMPL(log, level, content) \
  EXPECT_CALL(log,                                              \
              Log(logging::LOG_##level, _, _, _, testing::HasSubstr(content)))

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_MOCK_LOG_IMPL_H_
