// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/mailbox_frame_registry.h"

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
  return VideoFrameResource::Create(std::move(video_frame));
}

}  // namespace

class MailboxFrameRegistryTest : public ::testing::Test {
 public:
  MailboxFrameRegistryTest()
      : registry_(base::MakeRefCounted<MailboxFrameRegistry>()) {}
  MailboxFrameRegistryTest(const MailboxFrameRegistryTest&) = delete;
  MailboxFrameRegistryTest& operator=(const MailboxFrameRegistryTest&) = delete;
  ~MailboxFrameRegistryTest() override = default;

 protected:
  scoped_refptr<MailboxFrameRegistry> registry_;
};

// This tests registering a frame, accessing it, unregistering it, and ensuring
// that it is cleared from the registry.
TEST_F(MailboxFrameRegistryTest, RegisterAccessUnregister) {
  constexpr base::TimeDelta kTimestamp = base::Microseconds(42);
  auto frame = GenerateFrame(kTimestamp);
  ASSERT_TRUE(!!frame);
  // Transfer the reference to |frame| to the registry.
  const auto mailbox = registry_->RegisterFrame(std::move(frame));

  // We should be able to access the frame in the registry. Accessing the frame
  // creates a new reference in the returned value.
  scoped_refptr<const FrameResource> retrieved_frame =
      registry_->AccessFrame(mailbox);
  ASSERT_TRUE(!!retrieved_frame);
  EXPECT_EQ(kTimestamp, retrieved_frame->timestamp());

  // We can even retrieve it twice (which makes another reference).
  scoped_refptr<const FrameResource> retrieved_frame_two =
      registry_->AccessFrame(mailbox);
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
  registry_->UnregisterFrame(mailbox);
  EXPECT_TRUE(retrieved_frame->HasOneRef());

  // After unregistering a frame with its mailbox, that mailbox cannot be used
  // to access the frame again.
  ASSERT_DEATH({ registry_->AccessFrame(mailbox); }, "");
}

// This tests registering a frame, accessing it, and the frame's lifecycle.
TEST_F(MailboxFrameRegistryTest, CheckRegistryLifecycle) {
  constexpr base::TimeDelta kTimestamp = base::Microseconds(42);
  auto frame = GenerateFrame(kTimestamp);
  ASSERT_TRUE(!!frame);
  // Transfer the reference to |frame| to the registry.
  const auto mailbox = registry_->RegisterFrame(std::move(frame));

  // We should be able to access the frame in the registry. Accessing the frame
  // creates a new reference in the returned value.
  scoped_refptr<const FrameResource> retrieved_frame =
      registry_->AccessFrame(mailbox);
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

// The does a negative test of registering a frame with an unregistered mailbox.
TEST_F(MailboxFrameRegistryTest, InvalidFrameAccess) {
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  ASSERT_DEATH({ auto frame = registry_->AccessFrame(mailbox); }, "");
}

}  // namespace media
