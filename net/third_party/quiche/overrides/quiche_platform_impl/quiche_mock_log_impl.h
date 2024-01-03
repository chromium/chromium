// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_MOCK_LOG_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_MOCK_LOG_IMPL_H_

#include "base/test/mock_log.h"
#include "testing/gmock/include/gmock/gmock.h"  // IWYU pragma: export

using QuicheMockLogImpl = base::test::MockLog;

#define CREATE_QUICHE_MOCK_LOG_IMPL(log) QuicheMockLog log

#define EXPECT_QUICHE_LOG_CALL_IMPL(log) EXPECT_CALL(log, Log(_, _, _, _, _))

#define EXPECT_QUICHE_LOG_CALL_CONTAINS_IMPL(log, level, content) \
  EXPECT_CALL(log, Log(logging::LOGGING_##level, _, _, _,         \
                       testing::HasSubstr(content)))

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_MOCK_LOG_IMPL_H_
