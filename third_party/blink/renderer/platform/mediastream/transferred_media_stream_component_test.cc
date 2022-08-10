// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/transferred_media_stream_component.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_heap.h"

namespace blink {

class TransferredMediaStreamComponentTest : public testing::Test {
 public:
  void SetUp() override {
    transferred_component_ =
        MakeGarbageCollected<TransferredMediaStreamComponent>(
            TransferredMediaStreamComponent::TransferredValues{.id = "id"});
  }

  void TearDown() override { WebHeap::CollectAllGarbageForTesting(); }

  Persistent<TransferredMediaStreamComponent> transferred_component_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(TransferredMediaStreamComponentTest, InitialProperties) {
  EXPECT_EQ(transferred_component_->Id(), "id");
}

}  // namespace blink
