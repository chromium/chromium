// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_FAKE_CODEC_ALLOCATOR_H_
#define MEDIA_GPU_ANDROID_FAKE_CODEC_ALLOCATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/gpu/android/codec_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/surface_texture.h"

namespace media {

// A codec allocator that provides a configurable fake implementation
// and lets you set expecations on the "Mock*" methods.
class FakeCodecAllocator : public testing::NiceMock<CodecAllocator> {
 public:
  explicit FakeCodecAllocator(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  FakeCodecAllocator(const FakeCodecAllocator&) = delete;
  FakeCodecAllocator& operator=(const FakeCodecAllocator&) = delete;

  ~FakeCodecAllocator() override;

  // These are called with some parameters of the codec config by our
  // implementation of their respective functions.  This allows tests to set
  // expectations on them.
  MOCK_METHOD0(MockCreateMediaCodecAsync, void());

  // Note that this doesn't exactly match the signature, since unique_ptr
  // doesn't work.
  MOCK_METHOD1(MockReleaseMediaCodec, void(MediaCodecBridge*));

  void CreateMediaCodecAsync(CodecCreatedCB codec_created_cb,
                             std::unique_ptr<VideoCodecConfig> config) override;
  void ReleaseMediaCodec(std::unique_ptr<MediaCodecBridge> media_codec,
                         base::OnceClosure codec_released_cb) override;

  // Satisfies the pending codec creation with |codec| if given, or a new
  // MockMediaCodecBridge if not. Returns a raw pointer to the codec, or nullptr
  // if the client WeakPtr was invalidated.
  MockMediaCodecBridge* ProvideMockCodecAsync(
      std::unique_ptr<MockMediaCodecBridge> codec = nullptr);

  // Satisfies the pending codec creation with a null codec.
  void ProvideNullCodecAsync();

  // Most recent codec that we've created via CreateMockCodec, since we have
  // to assign ownership.  It may be freed already.
  raw_ptr<MockMediaCodecBridge> most_recent_codec = nullptr;

  // The DestructionObserver for |most_recent_codec|.
  std::unique_ptr<DestructionObserver> most_recent_codec_destruction_observer;

  // Copy of most of the fields in the most recent config, except for the ptrs.
  std::unique_ptr<VideoCodecConfig> most_recent_config;

 private:
  CodecCreatedCB pending_codec_created_cb_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_FAKE_CODEC_ALLOCATOR_H_
