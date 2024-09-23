// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TEST_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TEST_IMPL_H_

#include <cstdint>

#include "base/test/gtest_util.h"
#include "net/quic/platform/impl/quic_test_flags_utils.h"
#include "net/test/scoped_disable_exit_on_dfatal.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quiche/quic/platform/api/quic_flags.h"
#include "testing/gmock/include/gmock/gmock.h"      // IWYU pragma: export
#include "testing/gtest/include/gtest/gtest-spi.h"  // IWYU pragma: export
#include "testing/gtest/include/gtest/gtest.h"      // IWYU pragma: export

namespace quiche::test {

class QuicheTestImpl : public ::testing::Test {
 private:
  QuicFlagChecker checker_;
  QuicFlagSaverImpl saver_;  // Save/restore all QUIC flag values.
};

template <class T>
class QuicheTestWithParamImpl : public ::testing::TestWithParam<T> {
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

std::string QuicheGetCommonSourcePathImpl();

}  // namespace quiche::test

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
#define EXPECT_QUICHE_DEBUG_DEATH_IMPL(condition, message) \
  EXPECT_DEBUG_DEATH(condition, message)
#else
#define EXPECT_QUICHE_DEBUG_DEATH_IMPL(condition, message) \
  do {                                                     \
  } while (0)
#endif

#define EXPECT_QUICHE_DEATH_IMPL(condition, message) \
  EXPECT_CHECK_DEATH_WITH(condition, message)

#define QUICHE_TEST_DISABLED_IN_CHROME_IMPL(name) DISABLED_##name

#define QUICHE_SLOW_TEST_IMPL(name) DISABLED_##name

using QuicheFlagSaverImpl = QuicFlagSaverImpl;
using ScopedEnvironmentForThreadsImpl =
    quiche::test::ScopedEnvironmentForThreadsImpl;

std::string QuicheGetTestMemoryCachePathImpl();

using QuicheScopedDisableExitOnDFatalImpl =
    net::test::ScopedDisableExitOnDFatal;

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_TEST_IMPL_H_
