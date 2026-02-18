// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/renderer_process_id_mojom_traits.h"

#include "base/dcheck_is_on.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/originating_process_id.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(RendererProcessIdTest, RendererProcessValid) {
  RendererProcessId in(1);
  RendererProcessId out;

  EXPECT_TRUE(in);
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::RendererProcessId>(in, out));
  EXPECT_EQ(in, out);
}

TEST(RendererProcessIdTest, RendererProcessBrowser) {
  if (DCHECK_IS_ON()) {
    GTEST_SKIP() << "Passing 0 to RendererProcessId causes a DCHECK failure.";
  }
  RendererProcessId in(0);
  RendererProcessId out;

  EXPECT_FALSE(in);
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::RendererProcessId>(in, out));
}

TEST(RendererProcessIdTest, RendererProcessNegative) {
  RendererProcessId in(-2);
  RendererProcessId out;

  // Negative values are not invalid in the process due to the implementation
  // of base::IdType, but we explicitly disallow them from transmission.
  EXPECT_TRUE(in);
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::RendererProcessId>(in, out));
}

TEST(RendererProcessIdTest, RendererProcessInvalid) {
  RendererProcessId in;
  RendererProcessId out;

  EXPECT_FALSE(in);
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::RendererProcessId>(in, out));
}

}  // namespace network
