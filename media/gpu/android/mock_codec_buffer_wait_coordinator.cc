// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/mock_codec_buffer_wait_coordinator.h"

#include "gpu/command_buffer/service/ref_counted_lock_for_test.h"
#include "gpu/config/gpu_finch_features.h"

namespace media {

using testing::Invoke;
using testing::Return;

MockCodecBufferWaitCoordinator::MockCodecBufferWaitCoordinator(
    scoped_refptr<NiceMock<gpu::MockTextureOwner>> texture_owner)
    : CodecBufferWaitCoordinator(
          texture_owner,
          features::NeedThreadSafeAndroidMedia()
              ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
              : nullptr),
      mock_texture_owner(std::move(texture_owner)),
      expecting_frame_available(false) {
  ON_CALL(*this, texture_owner()).WillByDefault(Return(mock_texture_owner));

  ON_CALL(*this, SetReleaseTimeToNow())
      .WillByDefault(Invoke(
          this, &MockCodecBufferWaitCoordinator::FakeSetReleaseTimeToNow));
  ON_CALL(*this, IsExpectingFrameAvailable())
      .WillByDefault(Invoke(
          this,
          &MockCodecBufferWaitCoordinator::FakeIsExpectingFrameAvailable));
  ON_CALL(*this, WaitForFrameAvailable())
      .WillByDefault(Invoke(
          this, &MockCodecBufferWaitCoordinator::FakeWaitForFrameAvailable));
}

MockCodecBufferWaitCoordinator::~MockCodecBufferWaitCoordinator() = default;

}  // namespace media
