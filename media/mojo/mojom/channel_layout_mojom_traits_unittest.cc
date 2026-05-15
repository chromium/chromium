// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/channel_layout_mojom_traits.h"

#include "media/base/channel_layout.h"
#include "media/mojo/mojom/channel_layout.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

bool CanBeCreated(mojom::ChannelLayoutConfigPtr& input) {
  ChannelLayoutConfig output;
  return mojo::test::SerializeAndDeserialize<mojom::ChannelLayoutConfig>(
      input, output);
}

}  // namespace

TEST(ChannelLayoutConfigUnionTraitsTest, StereoRoundTrip) {
  auto input = mojom::ChannelLayoutConfig::NewPredefinedLayout(
      media::CHANNEL_LAYOUT_STEREO);
  EXPECT_TRUE(CanBeCreated(input));
}

TEST(ChannelLayoutConfigUnionTraitsTest, DiscreteRoundTrip) {
  auto input = mojom::ChannelLayoutConfig::NewDiscreteChannels(9);
  EXPECT_TRUE(CanBeCreated(input));
}

TEST(ChannelLayoutConfigUnionTraitsTest, DiscreteMustNotBeZeroChannels) {
  auto input = mojom::ChannelLayoutConfig::NewDiscreteChannels(0);
  EXPECT_FALSE(CanBeCreated(input));
}

TEST(ChannelLayoutConfigUnionTraitsTest, PredefinedLayoutMustNotBeDiscrete) {
  auto input = mojom::ChannelLayoutConfig::NewPredefinedLayout(
      media::CHANNEL_LAYOUT_DISCRETE);
  EXPECT_FALSE(CanBeCreated(input));
}

}  // namespace media
