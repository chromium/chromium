// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_TEST_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_TEST_IMPL_H_

#include "base/check_op.h"
#include "net/quic/platform/impl/quic_test_flags_utils.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "testing/gmock/include/gmock/gmock.h"      // IWYU pragma: export
#include "testing/gtest/include/gtest/gtest-spi.h"  // IWYU pragma: export
#include "testing/gtest/include/gtest/gtest.h"      // IWYU pragma: export


class QuicTestImpl : public ::testing::Test {
 private:
  QuicFlagChecker checker_;
  QuicFlagSaverImpl saver_;  // Save/restore all QUIC flag values.
};

template <class T>
class QuicTestWithParamImpl : public ::testing::TestWithParam<T> {
 private:
  QuicFlagChecker checker_;
  QuicFlagSaverImpl saver_;  // Save/restore all QUIC flag values.
};

class ScopedEnvironmentForThreadsImpl {
 public:
  ScopedEnvironmentForThreadsImpl()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

 public:
  base::test::TaskEnvironment task_environment_;
};

#define QUIC_TEST_DISABLED_IN_CHROME_IMPL(name) DISABLED_##name

std::string QuicGetTestMemoryCachePathImpl();

#define QUIC_SLOW_TEST_IMPL(name) DISABLED_##name

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_TEST_IMPL_H_
