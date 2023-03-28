// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/fuchsia_media_codec_provider_impl.h"

#include <fuchsia/mediacodec/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>
#include <algorithm>
#include <memory>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "media/base/media_util.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(320, 240);

const media::VideoDecoderConfig kH264BaseConfig(
    media::VideoCodec::kH264,
    media::H264PROFILE_MIN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    kCodedSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kUnencrypted);

const media::VideoDecoderConfig kVP9BaseConfig(
    media::VideoCodec::kVP9,
    media::VP9PROFILE_MIN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    kCodedSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kUnencrypted);

const fuchsia::mediacodec::CodecDescription kH264DecoderCodec = {
    .codec_type = fuchsia::mediacodec::CodecType::DECODER,
    .mime_type = "video/h264",
    .is_hw = true,
};

const fuchsia::mediacodec::CodecDescription kVP9DecoderCodec = {
    .codec_type = fuchsia::mediacodec::CodecType::DECODER,
    .mime_type = "video/vp9",
    .is_hw = true,
};

// Partial fake implementation of a CodecFactory
class FakeCodecFactory
    : public fuchsia::mediacodec::testing::CodecFactory_TestBase {
 public:
  explicit FakeCodecFactory(sys::OutgoingDirectory* outgoing_services)
      : binding_(outgoing_services, this) {}
  FakeCodecFactory(const FakeCodecFactory&) = delete;
  FakeCodecFactory& operator=(const FakeCodecFactory&) = delete;
  ~FakeCodecFactory() override = default;

  void TriggerOnCodecListEvent(
      std::vector<fuchsia::mediacodec::CodecDescription> codec_list) {
    binding_.events().OnCodecList(codec_list);
  }

  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "Unimplemented function called: " << name;
  }

 private:
  base::ScopedSingleClientServiceBinding<fuchsia::mediacodec::CodecFactory>
      binding_;
};

}  // namespace

class FuchsiaMediaCodecProviderImplTest : public testing::Test {
 public:
  FuchsiaMediaCodecProviderImplTest() = default;
  FuchsiaMediaCodecProviderImplTest(const FuchsiaMediaCodecProviderImplTest&) =
      delete;
  FuchsiaMediaCodecProviderImplTest& operator=(
      const FuchsiaMediaCodecProviderImplTest&) = delete;
  ~FuchsiaMediaCodecProviderImplTest() override = default;

 protected:
  std::unique_ptr<FuchsiaMediaCodecProviderImpl> CreateMediaCodecProvider() {
    auto* media_codec_provider = new FuchsiaMediaCodecProviderImpl();
    // Wait until event bindings are done.
    task_enviornment_.RunUntilIdle();
    return base::WrapUnique(media_codec_provider);
  }

  base::test::SingleThreadTaskEnvironment task_enviornment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  base::TestComponentContextForProcess component_context_;
};

TEST_F(FuchsiaMediaCodecProviderImplTest, NoMediaCodecConnection) {
  auto media_codec_provider = CreateMediaCodecProvider();
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&> future;

  media_codec_provider->GetSupportedVideoDecoderConfigs(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(FuchsiaMediaCodecProviderImplTest, DisconnectWhileGettingCodecList) {
  auto codec_factory_ptr = std::make_unique<FakeCodecFactory>(
      component_context_.additional_services());
  auto media_codec_provider = CreateMediaCodecProvider();
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&> future;

  codec_factory_ptr->TriggerOnCodecListEvent({kH264DecoderCodec});
  // Wait until the event is handled.
  task_enviornment_.RunUntilIdle();

  // Disconnect the service.
  codec_factory_ptr.reset();

  media_codec_provider->GetSupportedVideoDecoderConfigs(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(FuchsiaMediaCodecProviderImplTest, GetSupportedVideoDecoderConfigs) {
  FakeCodecFactory codec_factory(component_context_.additional_services());
  auto media_codec_provider = CreateMediaCodecProvider();
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&> future;

  media_codec_provider->GetSupportedVideoDecoderConfigs(future.GetCallback());
  // Wait until the callback is queued up.
  task_enviornment_.RunUntilIdle();

  codec_factory.TriggerOnCodecListEvent({kH264DecoderCodec});
  // Wait until the event is handled.
  task_enviornment_.RunUntilIdle();

  EXPECT_TRUE(
      media::IsVideoDecoderConfigSupported(future.Get(), kH264BaseConfig));
}

TEST_F(FuchsiaMediaCodecProviderImplTest,
       GetSupportedVideoDecoderConfigsInAQueue) {
  FakeCodecFactory codec_factory(component_context_.additional_services());
  auto media_codec_provider = CreateMediaCodecProvider();
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&> future_1;
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&> future_2;

  media_codec_provider->GetSupportedVideoDecoderConfigs(future_1.GetCallback());
  media_codec_provider->GetSupportedVideoDecoderConfigs(future_2.GetCallback());
  // Wait until the callbacks are queued up.
  task_enviornment_.RunUntilIdle();

  codec_factory.TriggerOnCodecListEvent({kH264DecoderCodec});
  // Wait until the event is handled.
  task_enviornment_.RunUntilIdle();

  EXPECT_TRUE(
      media::IsVideoDecoderConfigSupported(future_1.Get(), kH264BaseConfig));
  EXPECT_TRUE(
      media::IsVideoDecoderConfigSupported(future_2.Get(), kH264BaseConfig));
}

TEST_F(FuchsiaMediaCodecProviderImplTest,
       CodecListUpdatesWhileGettingSupportedVideoDecoderConfigs) {
  FakeCodecFactory codec_factory(component_context_.additional_services());
  auto media_codec_provider = CreateMediaCodecProvider();
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&> future_1;
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&> future_2;

  media_codec_provider->GetSupportedVideoDecoderConfigs(future_1.GetCallback());
  // Wait until the callback is queued up.
  task_enviornment_.RunUntilIdle();

  codec_factory.TriggerOnCodecListEvent({kH264DecoderCodec});
  // Wait until the event is handled.
  task_enviornment_.RunUntilIdle();

  codec_factory.TriggerOnCodecListEvent({kVP9DecoderCodec});
  // Wait until the event is handled.
  task_enviornment_.RunUntilIdle();
  media_codec_provider->GetSupportedVideoDecoderConfigs(future_2.GetCallback());
  EXPECT_TRUE(future_2.Wait());

  EXPECT_TRUE(
      media::IsVideoDecoderConfigSupported(future_1.Get(), kH264BaseConfig));
  EXPECT_FALSE(
      media::IsVideoDecoderConfigSupported(future_1.Get(), kVP9BaseConfig));
  EXPECT_TRUE(
      media::IsVideoDecoderConfigSupported(future_2.Get(), kVP9BaseConfig));
  EXPECT_FALSE(
      media::IsVideoDecoderConfigSupported(future_2.Get(), kH264BaseConfig));
}

}  // namespace content
