// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/originating_process_id_mojom_traits.h"

#include "base/dcheck_is_on.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/originating_process_id.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

TEST(OriginatingProcessIdTest, OriginatingProcessIdBrowser) {
  OriginatingProcessId in = OriginatingProcessId::browser();
  OriginatingProcessId out;

  EXPECT_TRUE(in);
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::OriginatingProcessId>(
      in, out));
  EXPECT_EQ(in, out);
}

TEST(OriginatingProcessIdTest, OriginatingProcessIdRenderer) {
  OriginatingProcessId in =
      OriginatingProcessId::renderer(RendererProcessId(1));
  OriginatingProcessId out;

  EXPECT_TRUE(in);
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::OriginatingProcessId>(
      in, out));
  EXPECT_EQ(in, out);
}

TEST(OriginatingProcessIdTest, OriginatingProcessIdRendererInvalid) {
  OriginatingProcessId in = OriginatingProcessId::renderer(RendererProcessId());
  OriginatingProcessId out;

  EXPECT_FALSE(in);
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::OriginatingProcessId>(
      in, out));
}

TEST(OriginatingProcessIdTest, OriginatingProcessIdRendererInvalidBrowser) {
  if (DCHECK_IS_ON()) {
    GTEST_SKIP() << "Passing 0 to RendererProcessId causes a DCHECK failure.";
  }
  OriginatingProcessId in =
      OriginatingProcessId::renderer(RendererProcessId(0));
  OriginatingProcessId out;

  EXPECT_FALSE(in);
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::OriginatingProcessId>(
      in, out));
}

TEST(OriginatingProcessIdTest, OriginatingProcessIdInvalid) {
  OriginatingProcessId in;
  OriginatingProcessId out;

  EXPECT_FALSE(in);
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::OriginatingProcessId>(
      in, out));
}

}  // namespace network
