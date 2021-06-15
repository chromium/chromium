// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class FencedFrameURLMappingTest : public ::testing::Test {
 public:
  void SetUp() override {}
};

TEST_F(FencedFrameURLMappingTest, AddAndConvert) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL test_url("https://foo.test");
  GURL urn_uuid = fenced_frame_url_mapping.AddFencedFrameURL(test_url);
  EXPECT_EQ(
      test_url,
      fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid).value());
}

TEST_F(FencedFrameURLMappingTest, NonExistentUUID) {
  FencedFrameURLMapping fenced_frame_url_mapping;
  GURL urn_uuid("urn:uuid:C36973B5E5D9DE59E4C4364F137B3C7A");
  absl::optional<GURL> result =
      fenced_frame_url_mapping.ConvertFencedFrameURNToURL(urn_uuid);
  EXPECT_EQ(absl::nullopt, result);
}

}  // namespace content
