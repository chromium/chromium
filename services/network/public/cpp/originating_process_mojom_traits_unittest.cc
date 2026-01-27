// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/originating_process_mojom_traits.h"

#include "base/dcheck_is_on.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/originating_process.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(OriginatingProcessTest, OriginatingProcessBrowser) {
  OriginatingProcess in = OriginatingProcess::browser();
  OriginatingProcess out;

  EXPECT_TRUE(in);
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::OriginatingProcess>(in, out));
  EXPECT_EQ(in, out);
}

TEST(OriginatingProcessTest, OriginatingProcessRenderer) {
  OriginatingProcess in = OriginatingProcess::renderer(RendererProcess(1));
  OriginatingProcess out;

  EXPECT_TRUE(in);
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::OriginatingProcess>(in, out));
  EXPECT_EQ(in, out);
}

TEST(OriginatingProcessTest, OriginatingProcessRendererInvalid) {
  OriginatingProcess in = OriginatingProcess::renderer(RendererProcess());
  OriginatingProcess out;

  EXPECT_FALSE(in);
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::OriginatingProcess>(in, out));
}

TEST(OriginatingProcessTest, OriginatingProcessRendererInvalidBrowser) {
  if (DCHECK_IS_ON()) {
    GTEST_SKIP() << "Passing 0 to RendererProcess causes a DCHECK failure.";
  }
  OriginatingProcess in = OriginatingProcess::renderer(RendererProcess(0));
  OriginatingProcess out;

  EXPECT_FALSE(in);
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::OriginatingProcess>(in, out));
}

TEST(OriginatingProcessTest, OriginatingProcessInvalid) {
  OriginatingProcess in;
  OriginatingProcess out;

  EXPECT_FALSE(in);
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::OriginatingProcess>(in, out));
}

}  // namespace network
