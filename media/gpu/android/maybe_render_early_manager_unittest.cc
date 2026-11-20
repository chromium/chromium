// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "media/gpu/android/video_frame_factory_impl.h"

#include "media/gpu/android/codec_image.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using testing::NiceMock;
using testing::Return;

// The dimensions for specifying MockImage behavior.
enum ImageKind { kTextureOwner, kOverlay };
enum Phase { kInCodec, kInFrontBuffer, kInvalidated };
enum Expectation { kRenderToFrontBuffer, kRenderToBackBuffer, kNone };

// A mock image with the same interface as CodecImage.
struct MockImage {
  MockImage(ImageKind kind, Phase phase, Expectation expectation) {
    ON_CALL(*this, is_texture_owner_backed())
        .WillByDefault(Return(kind == kTextureOwner));

    ON_CALL(*this, was_rendered_to_front_buffer())
        .WillByDefault(Return(phase == kInFrontBuffer));

    if (expectation == kRenderToFrontBuffer) {
      EXPECT_CALL(*this, RenderToFrontBuffer())
          .WillOnce(Return(phase != kInvalidated));
    } else {
      EXPECT_CALL(*this, RenderToFrontBuffer()).Times(0);
    }

    if (expectation == kRenderToBackBuffer) {
      EXPECT_CALL(*this, RenderToTextureOwnerBackBuffer())
          .WillOnce(Return(phase != kInvalidated));
    } else {
      EXPECT_CALL(*this, RenderToTextureOwnerBackBuffer()).Times(0);
    }
  }

  MOCK_METHOD0(was_rendered_to_front_buffer, bool());
  MOCK_METHOD0(is_texture_owner_backed, bool());
  MOCK_METHOD0(RenderToFrontBuffer, bool());
  MOCK_METHOD0(RenderToTextureOwnerBackBuffer, bool());
};

class MaybeRenderEarlyTest : public testing::Test {
 public:
  MaybeRenderEarlyTest() = default;
  ~MaybeRenderEarlyTest() override = default;

  void AddImage(ImageKind kind, Phase phase, Expectation expectation) {
    owned_images_.push_back(
        std::make_unique<NiceMock<MockImage>>(kind, phase, expectation));
    images_.push_back(owned_images_.back().get());
  }

  std::vector<std::unique_ptr<NiceMock<MockImage>>> owned_images_;
  std::vector<raw_ptr<MockImage, VectorExperimental>> images_;
};

TEST_F(MaybeRenderEarlyTest, EmptyVector) {
  internal::MaybeRenderEarly(&images_);
}

TEST_F(MaybeRenderEarlyTest, SingleUnrenderedSTImageIsRendered) {
  AddImage(kTextureOwner, kInCodec, Expectation::kRenderToFrontBuffer);
  internal::MaybeRenderEarly(&images_);
}

TEST_F(MaybeRenderEarlyTest, SingleUnrenderedOverlayImageIsRendered) {
  AddImage(kOverlay, kInCodec, Expectation::kRenderToFrontBuffer);
  internal::MaybeRenderEarly(&images_);
}

TEST_F(MaybeRenderEarlyTest, InvalidatedImagesAreSkippedOver) {
  AddImage(kTextureOwner, kInvalidated, Expectation::kRenderToFrontBuffer);
  AddImage(kTextureOwner, kInvalidated, Expectation::kRenderToFrontBuffer);
  AddImage(kTextureOwner, kInCodec, Expectation::kRenderToFrontBuffer);
  internal::MaybeRenderEarly(&images_);
}

// This also serves as a test that Overlay images are not back buffer rendered.
TEST_F(MaybeRenderEarlyTest, NoFrontBufferRenderingIfAlreadyPopulated) {
  AddImage(kOverlay, kInFrontBuffer, Expectation::kNone);
  AddImage(kOverlay, kInCodec, Expectation::kNone);
  internal::MaybeRenderEarly(&images_);
}

TEST_F(MaybeRenderEarlyTest,
       ImageFollowingLatestFrontBufferIsBackBufferRendered) {
  AddImage(kTextureOwner, kInCodec, Expectation::kNone);
  AddImage(kTextureOwner, kInFrontBuffer, Expectation::kNone);
  AddImage(kTextureOwner, kInCodec, Expectation::kRenderToBackBuffer);
  AddImage(kTextureOwner, kInCodec, Expectation::kNone);
  internal::MaybeRenderEarly(&images_);
}

}  // namespace media
