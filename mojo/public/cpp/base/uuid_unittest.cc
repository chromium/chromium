// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/uuid_mojom_traits.h"

#include "base/uuid.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/mojom/base/uuid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo_base {
namespace {

TEST(UuidTest, RandomV4Token) {
  base::Uuid in = base::Uuid::GenerateRandomV4();
  base::Uuid out;

  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Uuid>(in, out));
  EXPECT_EQ(in, out);
}

}  // namespace
}  // namespace mojo_base
