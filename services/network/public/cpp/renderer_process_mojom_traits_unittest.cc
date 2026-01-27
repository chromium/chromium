// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/renderer_process_mojom_traits.h"

#include "base/dcheck_is_on.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/originating_process.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(RendererProcessTest, RendererProcessValid) {
  RendererProcess in(1);
  RendererProcess out;

  EXPECT_TRUE(in);
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::RendererProcess>(in, out));
  EXPECT_EQ(in, out);
}

TEST(RendererProcessTest, RendererProcessBrowser) {
  if (DCHECK_IS_ON()) {
    GTEST_SKIP() << "Passing 0 to RendererProcess causes a DCHECK failure.";
  }
  RendererProcess in(0);
  RendererProcess out;

  EXPECT_FALSE(in);
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::RendererProcess>(in, out));
}

TEST(RendererProcessTest, RendererProcessNegative) {
  RendererProcess in(-2);
  RendererProcess out;

  // Negative values are not invalid in the process due to the implementation
  // of base::IdType, but we explicitly disallow them from transmission.
  EXPECT_TRUE(in);
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::RendererProcess>(in, out));
}

TEST(RendererProcessTest, RendererProcessInvalid) {
  RendererProcess in;
  RendererProcess out;

  EXPECT_FALSE(in);
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::RendererProcess>(in, out));
}

}  // namespace network
