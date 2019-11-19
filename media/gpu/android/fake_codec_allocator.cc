// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/fake_codec_allocator.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "media/base/android/mock_media_codec_bridge.h"
#include "media/gpu/android/codec_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

FakeCodecAllocator::FakeCodecAllocator(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : testing::NiceMock<CodecAllocator>(
          base::BindRepeating(&MockMediaCodecBridge::CreateVideoDecoder),
          std::move(task_runner)) {}

FakeCodecAllocator::~FakeCodecAllocator() = default;

void FakeCodecAllocator::CreateMediaCodecAsync(
    CodecCreatedCB codec_created_cb,
    std::unique_ptr<VideoCodecConfig> config) {
  // Clear |most_recent_codec| until somebody calls Provide*CodecAsync().
  most_recent_codec = nullptr;
  most_recent_codec_destruction_observer = nullptr;
  most_recent_config = std::move(config);
  pending_codec_created_cb_ = std::move(codec_created_cb);
  MockCreateMediaCodecAsync();
}

void FakeCodecAllocator::ReleaseMediaCodec(
    std::unique_ptr<MediaCodecBridge> media_codec,
    base::OnceClosure codec_released_cb) {
  std::move(codec_released_cb).Run();
  MockReleaseMediaCodec(media_codec.get());
}

MockMediaCodecBridge* FakeCodecAllocator::ProvideMockCodecAsync(
    std::unique_ptr<MockMediaCodecBridge> codec) {
  DCHECK(pending_codec_created_cb_);
  auto mock_codec = codec ? std::move(codec)
                          : std::make_unique<NiceMock<MockMediaCodecBridge>>();
  auto* raw_codec = mock_codec.get();
  most_recent_codec = raw_codec;
  most_recent_codec_destruction_observer =
      mock_codec->CreateDestructionObserver();

  std::move(pending_codec_created_cb_).Run(std::move(mock_codec));
  return raw_codec;
}

void FakeCodecAllocator::ProvideNullCodecAsync() {
  DCHECK(pending_codec_created_cb_);
  most_recent_codec = nullptr;
  std::move(pending_codec_created_cb_).Run(nullptr);
}

}  // namespace media
