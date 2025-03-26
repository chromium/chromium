// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/load_timing_internal_info_mojom_traits.h"

#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/load_timing_internal_info.h"
#include "services/network/public/mojom/load_timing_internal_info.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(LoadTimingInternalInfoMojomTraitsTest, SerializeAndDeserialize) {
  net::LoadTimingInternalInfo original;
  original.initialize_stream_delay = base::Seconds(1);

  net::LoadTimingInternalInfo deserialized;
  ASSERT_NE(deserialized, original);
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::LoadTimingInternalInfo>(
          original, deserialized));
  EXPECT_EQ(deserialized, original);
}

}  // namespace
}  // namespace network
