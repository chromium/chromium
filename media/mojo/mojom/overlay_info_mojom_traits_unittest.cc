// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/overlay_info_mojom_traits.h"

#include "base/unguessable_token.h"
#include "media/base/overlay_info.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(OverlayInfoMojomTraitsTest, SerializeAndDeserialize) {
  OverlayInfo input;
  input.routing_token = base::UnguessableToken::Create();
  input.is_fullscreen = true;
  input.is_persistent_video = true;

  OverlayInfo output;
  mojo::test::SerializeAndDeserialize<mojom::OverlayInfo>(input, output);

  EXPECT_EQ(input.routing_token, output.routing_token);
  EXPECT_EQ(input.is_fullscreen, output.is_fullscreen);
  EXPECT_EQ(input.is_persistent_video, output.is_persistent_video);
}

TEST(OverlayInfoMojomTraitsTest, SerializeAndDeserialize_NoToken) {
  OverlayInfo input;
  input.routing_token = std::nullopt;
  input.is_fullscreen = false;
  input.is_persistent_video = false;

  OverlayInfo output;
  mojo::test::SerializeAndDeserialize<mojom::OverlayInfo>(input, output);

  EXPECT_EQ(input.routing_token, output.routing_token);
  EXPECT_EQ(input.is_fullscreen, output.is_fullscreen);
  EXPECT_EQ(input.is_persistent_video, output.is_persistent_video);
}

}  // namespace media
