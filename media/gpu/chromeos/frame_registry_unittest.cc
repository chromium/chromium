// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/frame_registry.h"

#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {
// A helper to generate a VideoFrame-backed FrameResource.
scoped_refptr<FrameResource> GenerateFrame(base::TimeDelta timestamp) {
  constexpr gfx::Size kCodedSize(64, 48);
  constexpr gfx::Rect kVisibleRect(64, 30);
  constexpr gfx::Size kNaturalSize(120, 60);
  constexpr VideoPixelFormat kPixelFormat = PIXEL_FORMAT_I420;

  auto video_frame = VideoFrame::CreateFrame(
      kPixelFormat, kCodedSize, kVisibleRect, kNaturalSize, timestamp);
  video_frame->metadata().tracking_token = base::UnguessableToken::Create();
  return VideoFrameResource::Create(std::move(video_frame));
}

}  // namespace

class FrameRegistryTest : public ::testing::Test {
 public:
  FrameRegistryTest() : registry_(base::MakeRefCounted<FrameRegistry>()) {}
  FrameRegistryTest(const FrameRegistryTest&) = delete;
  FrameRegistryTest& operator=(const FrameRegistryTest&) = delete;
  ~FrameRegistryTest() override = default;

 protected:
  scoped_refptr<FrameRegistry> registry_;
};

// This tests registering a frame, accessing it, unregistering it, and ensuring
// that it is cleared from the registry.
TEST_F(FrameRegistryTest, RegisterAccessUnregister) {
  constexpr base::TimeDelta kTimestamp = base::Microseconds(42);
  auto frame = GenerateFrame(kTimestamp);
  const base::UnguessableToken token = frame->tracking_token();

  ASSERT_TRUE(!!frame);
  // Transfer the reference to |frame| to the registry.
  registry_->RegisterFrame(std::move(frame));

  // We should be able to access the frame in the registry. Accessing the frame
  // creates a new reference in the returned value.
  scoped_refptr<const FrameResource> retrieved_frame =
      registry_->AccessFrame(token);
  ASSERT_TRUE(!!retrieved_frame);
  EXPECT_EQ(kTimestamp, retrieved_frame->timestamp());

  // We can even retrieve it twice (which makes another reference).
  scoped_refptr<const FrameResource> retrieved_frame_two =
      registry_->AccessFrame(token);
  ASSERT_TRUE(!!retrieved_frame_two);
  EXPECT_EQ(kTimestamp, retrieved_frame_two->timestamp());

  // Resets the second retrieved frame to drop the reference count.
  retrieved_frame_two.reset();

  // At this point in time, there should be 2 references to the original frame:
  // one in |retrieved_frame| and one in |registry_|. This checks that there are
  // at least two.
  EXPECT_TRUE(retrieved_frame->HasAtLeastOneRef() &&
              !retrieved_frame->HasOneRef());

  // Resetting the registry should release one reference to the frame. Now,
  // there should be exactly one reference.
  registry_->UnregisterFrame(token);
  EXPECT_TRUE(retrieved_frame->HasOneRef());

  // After unregistering a frame with its token, that token cannot be used
  // to access the frame again.
  ASSERT_DEATH({ registry_->AccessFrame(token); }, "");
}

// This tests registering a frame, accessing it, and the frame's lifecycle.
TEST_F(FrameRegistryTest, CheckRegistryLifecycle) {
  constexpr base::TimeDelta kTimestamp = base::Microseconds(42);
  auto frame = GenerateFrame(kTimestamp);
  const base::UnguessableToken token = frame->tracking_token();

  ASSERT_TRUE(!!frame);
  // Transfer the reference to |frame| to the registry.
  registry_->RegisterFrame(std::move(frame));

  // We should be able to access the frame in the registry. Accessing the frame
  // creates a new reference in the returned value.
  scoped_refptr<const FrameResource> retrieved_frame =
      registry_->AccessFrame(token);
  ASSERT_TRUE(!!retrieved_frame);
  EXPECT_EQ(kTimestamp, retrieved_frame->timestamp());

  // At this point in time, there should be 2 references to the original frame:
  // one in |retrieved_frame| and one in |registry_|. This checks that there are
  // at least two.
  EXPECT_TRUE(retrieved_frame->HasAtLeastOneRef() &&
              !retrieved_frame->HasOneRef());

  // Resetting the registry should release one reference to the frame. Now,
  // there should be exactly one reference.
  registry_.reset();
  EXPECT_TRUE(retrieved_frame->HasOneRef());
}

// The does a negative test of registering a frame with an unregistered token.
TEST_F(FrameRegistryTest, InvalidFrameAccess) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  ASSERT_DEATH({ auto frame = registry_->AccessFrame(token); }, "");
}

}  // namespace media
