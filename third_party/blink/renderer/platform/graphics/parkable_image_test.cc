// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/parkable_image.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

namespace {
const char gABC[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char g123[] = "1234567890";
}  // namespace

TEST(ParkableImageTest, Size) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto pi = ParkableImage::Create();

  EXPECT_EQ(pi->size(), 0u);

  // This has capacity 10, not size 10; size should still be 0.
  pi = ParkableImage::Create(10);

  EXPECT_EQ(pi->size(), 0u);
}

TEST(ParkableImageTest, Append) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);  // Should be empty when created.

  pi->Append(WTF::SharedBuffer::Create(gABC, sizeof(gABC)).get(), 0);

  EXPECT_EQ(pi->size(), sizeof(gABC));
}

TEST(ParkableImageTest, AppendMultiple) {
  base::test::SingleThreadTaskEnvironment task_environment;

  auto pi = ParkableImage::Create();
  ASSERT_EQ(pi->size(), 0u);  // Should be empty when created.

  auto sb = WTF::SharedBuffer::Create(gABC, sizeof(gABC));
  ASSERT_EQ(sb->size(), sizeof(gABC));

  pi->Append(sb.get(), 0);

  EXPECT_EQ(pi->size(), sizeof(gABC));

  sb->Append(g123, sizeof(g123));
  ASSERT_EQ(sb->size(), sizeof(g123) + sizeof(gABC));

  pi->Append(sb.get(), pi->size());

  EXPECT_EQ(pi->size(), sizeof(gABC) + sizeof(g123));
}

}  // namespace blink
