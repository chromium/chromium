// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_TEST_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_TEST_IMPL_H_

#include "base/logging.h"
#include "net/test/test_with_task_environment.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "testing/gmock/include/gmock/gmock.h"      // IWYU pragma: export
#include "testing/gtest/include/gtest/gtest-spi.h"  // IWYU pragma: export
#include "testing/gtest/include/gtest/gtest.h"      // IWYU pragma: export

// When constructed, saves the current values of all QUIC flags. When
// destructed, restores all QUIC flags to the saved values.
class QuicFlagSaverImpl {
 public:
  QuicFlagSaverImpl();
  ~QuicFlagSaverImpl();

 private:
#define QUIC_FLAG(type, flag, value) type saved_##flag##_;
#include "net/quic/quic_flags_list.h"
#undef QUIC_FLAG
};

// Checks if all QUIC flags are on their default values on construction.
class QuicFlagChecker {
 public:
  QuicFlagChecker() {
#define QUIC_FLAG(type, flag, value)                                      \
  CHECK_EQ(value, flag)                                                   \
      << "Flag set to an unexpected value.  A prior test is likely "      \
      << "setting a flag without using a QuicFlagSaver. Use QuicTest to " \
         "avoid this issue.";
#include "net/quic/quic_flags_list.h"
#undef QUIC_FLAG
  }
};

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

namespace quic {
// A utility function that returns all versions except v99.  Intended to be a
// drop-in replacement for quic::AllSupportedVersion() when disabling v99 in a
// large test file is required.
//
// TODO(vasilvv): all of the tests should be fixed for v99, so that this
// function can be removed.
ParsedQuicVersionVector AllVersionsExcept99();
}  // namespace quic

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
#define EXPECT_QUIC_DEBUG_DEATH_IMPL(condition, message) \
  EXPECT_DEBUG_DEATH(condition, message)
#else
#define EXPECT_QUIC_DEBUG_DEATH_IMPL(condition, message) \
  do {                                                   \
  } while (0);
#endif

#define QUIC_SLOW_TEST_IMPL(name) DISABLED_##name

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_TEST_IMPL_H_
