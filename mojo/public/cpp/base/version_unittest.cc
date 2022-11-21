// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/version.h"
#include "mojo/public/cpp/base/version_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/version.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {

TEST(VersionStructTraitsTest, InvalidVersion) {
  base::Version in;
  base::Version out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Version>(in, out));
  EXPECT_FALSE(out.IsValid());
}

TEST(VersionStructTraitsTest, ValidVersion) {
  base::Version in("9.8.7.6");
  base::Version out;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Version>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace mojo_base
