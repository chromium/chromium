// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/allocator_ort.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace webnn::ort {

class WebNNAllocatorOrtTest : public testing::Test {};

TEST_F(WebNNAllocatorOrtTest, GetInstance) {
  EXPECT_TRUE(AllocatorOrt::GetInstance());
}

}  // namespace webnn::ort
