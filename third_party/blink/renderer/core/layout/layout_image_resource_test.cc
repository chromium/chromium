// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_image_resource.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutImageResourceTest : public RenderingTest {
 public:
 protected:
};

TEST_F(LayoutImageResourceTest, BrokenImageHighRes) {
  EXPECT_NE(LayoutImageResource::BrokenImage(2.0),
            LayoutImageResource::BrokenImage(1.0));
}

}  // namespace blink
