// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_TEST_IMPL_H_
#define NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_TEST_IMPL_H_

#include "testing/gmock/include/gmock/gmock.h"  // IWYU pragma: export
#include "testing/gtest/include/gtest/gtest.h"  // IWYU pragma: export

namespace quiche {
namespace test {

class QuicheTest : public ::testing::Test {};

template <class T>
class QuicheTestWithParamImpl : public ::testing::TestWithParam<T> {};

std::string QuicheGetCommonSourcePathImpl();

}  // namespace test
}  // namespace quiche

#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG)
#define EXPECT_QUICHE_DEBUG_DEATH_IMPL(condition, message) \
  EXPECT_DEBUG_DEATH(condition, message)
#else
#define EXPECT_QUICHE_DEBUG_DEATH_IMPL(condition, message) \
  do {                                                     \
  } while (0)
#endif

#endif  // NET_QUICHE_COMMON_PLATFORM_IMPL_QUICHE_TEST_IMPL_H_
