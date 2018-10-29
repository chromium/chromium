// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/fake_codec_allocator.h"

#include <memory>

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
          task_runner),
      most_recent_config(new CodecConfig()) {}

FakeCodecAllocator::~FakeCodecAllocator() = default;

void FakeCodecAllocator::StartThread(CodecAllocatorClient* client) {}

void FakeCodecAllocator::StopThread(CodecAllocatorClient* client) {}

std::unique_ptr<MediaCodecBridge> FakeCodecAllocator::CreateMediaCodecSync(
    scoped_refptr<CodecConfig> config) {
  CopyCodecConfig(config);
  MockCreateMediaCodecSync(most_recent_overlay, most_recent_texture_owner);

  std::unique_ptr<MockMediaCodecBridge> codec;
  if (allow_sync_creation) {
    codec = std::make_unique<MockMediaCodecBridge>();
    most_recent_codec = codec.get();
    most_recent_codec_destruction_observer = codec->CreateDestructionObserver();
    most_recent_codec_destruction_observer->DoNotAllowDestruction();
  } else {
    most_recent_codec = nullptr;
    most_recent_codec_destruction_observer = nullptr;
  }

  return std::move(codec);
}

void FakeCodecAllocator::CreateMediaCodecAsync(
    base::WeakPtr<CodecAllocatorClient> client,
    scoped_refptr<CodecConfig> config) {
  // Clear |most_recent_codec| until somebody calls Provide*CodecAsync().
  most_recent_codec = nullptr;
  most_recent_codec_destruction_observer = nullptr;
  CopyCodecConfig(config);
  pending_surface_bundle_ = config->surface_bundle;
  client_ = client;
  codec_creation_pending_ = true;

  MockCreateMediaCodecAsync(most_recent_overlay, most_recent_texture_owner);
}

void FakeCodecAllocator::ReleaseMediaCodec(
    std::unique_ptr<MediaCodecBridge> media_codec,
    scoped_refptr<AVDASurfaceBundle> surface_bundle) {
  MockReleaseMediaCodec(media_codec.get(), surface_bundle->overlay.get(),
                        surface_bundle->texture_owner_.get());
}

MockMediaCodecBridge* FakeCodecAllocator::ProvideMockCodecAsync(
    std::unique_ptr<MockMediaCodecBridge> codec) {
  DCHECK(codec_creation_pending_);
  codec_creation_pending_ = false;

  if (!client_)
    return nullptr;

  auto mock_codec = codec ? std::move(codec)
                          : std::make_unique<NiceMock<MockMediaCodecBridge>>();
  auto* raw_codec = mock_codec.get();
  most_recent_codec = raw_codec;
  most_recent_codec_destruction_observer =
      mock_codec->CreateDestructionObserver();
  client_->OnCodecConfigured(std::move(mock_codec),
                             std::move(pending_surface_bundle_));
  return raw_codec;
}

void FakeCodecAllocator::ProvideNullCodecAsync() {
  DCHECK(codec_creation_pending_);
  codec_creation_pending_ = false;
  most_recent_codec = nullptr;
  if (client_)
    client_->OnCodecConfigured(nullptr, std::move(pending_surface_bundle_));
}

void FakeCodecAllocator::CopyCodecConfig(scoped_refptr<CodecConfig> config) {
  // CodecConfig isn't copyable, since it has unique_ptrs and such.
  most_recent_overlay = config->surface_bundle->overlay.get();
  most_recent_texture_owner = config->surface_bundle->texture_owner_.get();
  most_recent_config->media_crypto =
      config->media_crypto
          ? std::make_unique<base::android::ScopedJavaGlobalRef<jobject>>(
                *config->media_crypto)
          : nullptr;
  most_recent_config->requires_secure_codec = config->requires_secure_codec;
  most_recent_config->initial_expected_coded_size =
      config->initial_expected_coded_size;
  most_recent_config->software_codec_forbidden =
      config->software_codec_forbidden;
  most_recent_config->csd0 = config->csd0;
  most_recent_config->csd1 = config->csd1;
}

}  // namespace media
