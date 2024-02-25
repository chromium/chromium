// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/fuchsia_media_codec_provider_impl.h"

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl.h>
#include <fuchsia/mediacodec/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>
#include <algorithm>
#include <memory>
#include <vector>

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

static const gfx::Size k320CodecSize(568, 320);
static const gfx::Size k1080CodecSize(1920, 1080);
static const gfx::Size k4kCodecSize(3840, 2160);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(320, 240);

// Encrypted VideoDecoderConfig
const media::VideoDecoderConfig kEncryptedH264Base1080Config(
    media::VideoCodec::kH264,
    media::H264PROFILE_MIN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    k1080CodecSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kCbcs);

const media::VideoDecoderConfig kEncryptedVP9BaseConfig(
    media::VideoCodec::kVP9,
    media::VP9PROFILE_MIN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    k1080CodecSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kCbcs);

const media::VideoDecoderConfig kEncryptedVP9MaxConfig(
    media::VideoCodec::kVP9,
    media::VP9PROFILE_MAX,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    k4kCodecSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kCbcs);

// Unencrypted VideoDecoderConfig
const media::VideoDecoderConfig kUnencryptedH264Base320Config(
    media::VideoCodec::kH264,
    media::H264PROFILE_MIN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    k320CodecSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kUnencrypted);

const media::VideoDecoderConfig kUnencryptedH264Base1080Config(
    media::VideoCodec::kH264,
    media::H264PROFILE_MIN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    k1080CodecSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kUnencrypted);

const media::VideoDecoderConfig kUnencryptedH264Base4kConfig(
    media::VideoCodec::kH264,
    media::H264PROFILE_MIN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    k4kCodecSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kUnencrypted);

const media::VideoDecoderConfig kUnencryptedH264High4kConfig(
    media::VideoCodec::kH264,
    media::H264PROFILE_HIGH,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    k4kCodecSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kUnencrypted);

const media::VideoDecoderConfig kUnencryptedVP9BaseConfig(
    media::VideoCodec::kVP9,
    media::VP9PROFILE_MIN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    k1080CodecSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kUnencrypted);

const media::VideoDecoderConfig kUnknownConfig(
    media::VideoCodec::kUnknown,
    media::VIDEO_CODEC_PROFILE_UNKNOWN,
    media::VideoDecoderConfig::AlphaMode::kIsOpaque,
    media::VideoColorSpace(),
    media::kNoTransformation,
    k4kCodecSize,
    kVisibleRect,
    kNaturalSize,
    media::EmptyExtraData(),
    media::EncryptionScheme::kUnencrypted);

const fuchsia::math::SizeU k320CodedSize = {
    .width = 568,
    .height = 320,
};

const fuchsia::math::SizeU k480CodedSize = {
    .width = 852,
    .height = 480,
};

const fuchsia::math::SizeU k1080CodedSize = {
    .width = 1920,
    .height = 1080,
};

const fuchsia::math::SizeU k4kCodedSize = {
    .width = 3840,
    .height = 2160,
};

class FakeCodecFactoryWithNoCodecs
    : public fuchsia::mediacodec::testing::CodecFactory_TestBase {
 public:
  explicit FakeCodecFactoryWithNoCodecs(
      sys::OutgoingDirectory* outgoing_services)
      : binding_(outgoing_services, this) {}
  FakeCodecFactoryWithNoCodecs(const FakeCodecFactoryWithNoCodecs&) = delete;
  FakeCodecFactoryWithNoCodecs& operator=(const FakeCodecFactoryWithNoCodecs&) =
      delete;
  ~FakeCodecFactoryWithNoCodecs() override = default;

  void GetDetailedCodecDescriptions(
      GetDetailedCodecDescriptionsCallback callback) override {
    callback(std::move(
        fuchsia::mediacodec::CodecFactoryGetDetailedCodecDescriptionsResponse()
            .set_codecs({})));
  }

  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "Unimplemented function called: " << name;
  }

 protected:
  base::ScopedSingleClientServiceBinding<fuchsia::mediacodec::CodecFactory>
      binding_;
};

class FakeCodecFactory : public FakeCodecFactoryWithNoCodecs {
 public:
  explicit FakeCodecFactory(sys::OutgoingDirectory* outgoing_services)
      : FakeCodecFactoryWithNoCodecs(outgoing_services) {}

  void GetDetailedCodecDescriptions(
      GetDetailedCodecDescriptionsCallback callback) override {
    requested_count_++;
    std::vector<fuchsia::mediacodec::DetailedCodecDescription> codec_list;
    codec_list.push_back(GetAudioDetailedCodecDescription());
    codec_list.push_back(GetSwDetailedCodecDescription());
    codec_list.push_back(GetUnknownDetailedCodecDescription());
    codec_list.push_back(GetUnencryptedH264DetailedCodecDescription());
    codec_list.push_back(GetEncryptedVP9DetailedCodecDescription());
    callback(std::move(
        fuchsia::mediacodec::CodecFactoryGetDetailedCodecDescriptionsResponse()
            .set_codecs(std::move(codec_list))));
  }

  // Gets the number of calls to `GetDetailedCodecDescriptions`.
  int GetRequestedCount() { return requested_count_; }

 private:
  fuchsia::mediacodec::DetailedCodecDescription
  GetAudioDetailedCodecDescription() {
    std::vector<fuchsia::mediacodec::DecoderProfileDescription> profile_list;

    profile_list.push_back(
        std::move(fuchsia::mediacodec::DecoderProfileDescription()
                      .set_split_header_handling(false)));

    return std::move(
        fuchsia::mediacodec::DetailedCodecDescription()
            .set_codec_type(fuchsia::mediacodec::CodecType::DECODER)
            .set_mime_type("audio/aac")
            .set_is_hw(true)
            .set_profile_descriptions(
                std::move(fuchsia::mediacodec::ProfileDescriptions()
                              .set_decoder_profile_descriptions(
                                  std::move(profile_list)))));
  }

  // Returns a software decoder codec that has a higher profile and a larger
  // image size range than `GetUnencryptedH264DetailedCodecDescription` returns.
  fuchsia::mediacodec::DetailedCodecDescription
  GetSwDetailedCodecDescription() {
    std::vector<fuchsia::mediacodec::DecoderProfileDescription> profile_list;

    profile_list.push_back(std::move(
        fuchsia::mediacodec::DecoderProfileDescription()
            .set_profile(fuchsia::media::CodecProfile::H264PROFILE_HIGH)
            .set_max_image_size(k4kCodedSize)
            .set_min_image_size(k320CodedSize)));

    return std::move(
        fuchsia::mediacodec::DetailedCodecDescription()
            .set_codec_type(fuchsia::mediacodec::CodecType::DECODER)
            .set_mime_type("video/h264")
            .set_is_hw(false)
            .set_profile_descriptions(
                std::move(fuchsia::mediacodec::ProfileDescriptions()
                              .set_decoder_profile_descriptions(
                                  std::move(profile_list)))));
  }

  fuchsia::mediacodec::DetailedCodecDescription
  GetUnknownDetailedCodecDescription() {
    std::vector<fuchsia::mediacodec::DecoderProfileDescription> profile_list;

    profile_list.push_back(
        std::move(fuchsia::mediacodec::DecoderProfileDescription()
                      .set_profile(fuchsia::media::CodecProfile::MJPEG_BASELINE)
                      .set_max_image_size(k4kCodedSize)
                      .set_min_image_size(k320CodedSize)));

    return std::move(
        fuchsia::mediacodec::DetailedCodecDescription()
            .set_codec_type(fuchsia::mediacodec::CodecType::DECODER)
            .set_mime_type("video/unknown")
            .set_is_hw(true)
            .set_profile_descriptions(
                std::move(fuchsia::mediacodec::ProfileDescriptions()
                              .set_decoder_profile_descriptions(
                                  std::move(profile_list)))));
  }

  fuchsia::mediacodec::DetailedCodecDescription
  GetUnencryptedH264DetailedCodecDescription() {
    std::vector<fuchsia::mediacodec::DecoderProfileDescription> profile_list;

    profile_list.push_back(std::move(
        fuchsia::mediacodec::DecoderProfileDescription()
            .set_profile(fuchsia::media::CodecProfile::H264PROFILE_BASELINE)
            .set_max_image_size(k1080CodedSize)
            .set_min_image_size(k480CodedSize)));

    return std::move(
        fuchsia::mediacodec::DetailedCodecDescription()
            .set_codec_type(fuchsia::mediacodec::CodecType::DECODER)
            .set_mime_type("video/h264")
            .set_is_hw(true)
            .set_profile_descriptions(
                std::move(fuchsia::mediacodec::ProfileDescriptions()
                              .set_decoder_profile_descriptions(
                                  std::move(profile_list)))));
  }

  fuchsia::mediacodec::DetailedCodecDescription
  GetEncryptedVP9DetailedCodecDescription() {
    std::vector<fuchsia::mediacodec::DecoderProfileDescription> profile_list;

    profile_list.push_back(std::move(
        fuchsia::mediacodec::DecoderProfileDescription()
            .set_profile(fuchsia::media::CodecProfile::VP9PROFILE_PROFILE0)
            .set_max_image_size(k1080CodedSize)
            .set_min_image_size(k480CodedSize)
            .set_allow_encryption(true)
            .set_require_encryption(true)));

    return std::move(
        fuchsia::mediacodec::DetailedCodecDescription()
            .set_codec_type(fuchsia::mediacodec::CodecType::DECODER)
            .set_mime_type("video/vp9")
            .set_is_hw(true)
            .set_profile_descriptions(
                std::move(fuchsia::mediacodec::ProfileDescriptions()
                              .set_decoder_profile_descriptions(
                                  std::move(profile_list)))));
  }

  // Number of calls to `GetDetailedCodecDescriptions`.
  int requested_count_ = 0;
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
  mojo::Remote<media::mojom::FuchsiaMediaCodecProvider> GetProviderRemote() {
    mojo::Remote<media::mojom::FuchsiaMediaCodecProvider>
        media_codec_provider_remote;
    media_codec_provider_.emplace();
    media_codec_provider_->AddReceiver(
        media_codec_provider_remote.BindNewPipeAndPassReceiver());
    return media_codec_provider_remote;
  }

  base::test::SingleThreadTaskEnvironment task_enviornment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  base::TestComponentContextForProcess component_context_;
  std::optional<FuchsiaMediaCodecProviderImpl> media_codec_provider_;
};

TEST_F(FuchsiaMediaCodecProviderImplTest, MissingCodecFactory_ReportsNoCodecs) {
  // No `CodecFactory` is published hence there will be no successful media
  // codec connection.
  auto provider_remote = GetProviderRemote();
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&>
      get_configs_future;

  provider_remote->GetSupportedVideoDecoderConfigs(
      get_configs_future.GetCallback());
  EXPECT_TRUE(get_configs_future.Get().empty());
}

TEST_F(FuchsiaMediaCodecProviderImplTest,
       DisconnectCodecFactoryDuringQuery_ReportsNoCodecs) {
  auto codec_factory_ptr = std::make_unique<FakeCodecFactory>(
      component_context_.additional_services());
  auto provider_remote = GetProviderRemote();

  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&>
      get_configs_future;
  provider_remote->GetSupportedVideoDecoderConfigs(
      get_configs_future.GetCallback());

  // Disconnect the service.
  codec_factory_ptr.reset();

  EXPECT_TRUE(get_configs_future.Get().empty());
}

TEST_F(FuchsiaMediaCodecProviderImplTest,
       CodecFactoryWithNoCodecs_ReportsNoCodecs) {
  FakeCodecFactoryWithNoCodecs codec_factory(
      component_context_.additional_services());
  auto provider_remote = GetProviderRemote();
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&>
      get_configs_future;

  provider_remote->GetSupportedVideoDecoderConfigs(
      get_configs_future.GetCallback());
  EXPECT_TRUE(get_configs_future.Get().empty());
}

TEST_F(FuchsiaMediaCodecProviderImplTest, GetSupportedVideoDecoderConfigs) {
  FakeCodecFactory codec_factory(component_context_.additional_services());
  auto provider_remote = GetProviderRemote();
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&>
      get_configs_future;

  provider_remote->GetSupportedVideoDecoderConfigs(
      get_configs_future.GetCallback());

  // Only the VP9 codec config from `GetEncryptedVP9DetailedCodecDescription`
  // and the H264 codec config from `GetUnencryptedH264DetailedCodecDescription`
  // are supported by the FakeCodecFactory. Ensure that entries are not added
  // for the audio, unknown and software codecs.
  const size_t kExpectedNumSupportedConfigs = 2;
  EXPECT_EQ(get_configs_future.Get().size(), kExpectedNumSupportedConfigs);

  // The H264 codec config from `GetUnencryptedH264DetailedCodecDescription`
  // does not support encryption.
  EXPECT_TRUE(media::IsVideoDecoderConfigSupported(
      get_configs_future.Get(), kUnencryptedH264Base1080Config));
  EXPECT_FALSE(media::IsVideoDecoderConfigSupported(
      get_configs_future.Get(), kEncryptedH264Base1080Config));

  // The VP9 codec config from `GetEncryptedVP9DetailedCodecDescription`
  // requires (not just supports) encryption.
  EXPECT_TRUE(media::IsVideoDecoderConfigSupported(get_configs_future.Get(),
                                                   kEncryptedVP9BaseConfig));
  EXPECT_FALSE(media::IsVideoDecoderConfigSupported(get_configs_future.Get(),
                                                    kUnencryptedVP9BaseConfig));

  // FakeCodecFactory does not support hardware-accelerated H264 High profile
  // video codec or VP9 max codec is supported.
  EXPECT_FALSE(media::IsVideoDecoderConfigSupported(
      get_configs_future.Get(), kUnencryptedH264High4kConfig));
  EXPECT_FALSE(media::IsVideoDecoderConfigSupported(get_configs_future.Get(),
                                                    kEncryptedVP9MaxConfig));

  // FakeCodecFactory only supports H264 Base profile from 480p to 1080p.
  // It supports a software H264 Base profile from 320p to 4k, but it should be
  // ignored.
  EXPECT_FALSE(media::IsVideoDecoderConfigSupported(
      get_configs_future.Get(), kUnencryptedH264Base320Config));
  EXPECT_FALSE(media::IsVideoDecoderConfigSupported(
      get_configs_future.Get(), kUnencryptedH264Base4kConfig));

  // Unknown video decoder config should not be supported.
  EXPECT_FALSE(media::IsVideoDecoderConfigSupported(get_configs_future.Get(),
                                                    kUnknownConfig));
}

TEST_F(FuchsiaMediaCodecProviderImplTest,
       GetSupportedVideoDecoderConfigsInAQueue) {
  FakeCodecFactory codec_factory(component_context_.additional_services());
  auto provider_remote = GetProviderRemote();
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&> future_1;
  base::test::TestFuture<const media::SupportedVideoDecoderConfigs&> future_2;

  // No RunLoop is spun until `TestFuture::Get()` is called.
  provider_remote->GetSupportedVideoDecoderConfigs(future_1.GetCallback());
  provider_remote->GetSupportedVideoDecoderConfigs(future_2.GetCallback());

  // Makes sure the callbacks are queued up before any completion from the
  // CodecFactory.
  EXPECT_EQ(codec_factory.GetRequestedCount(), 0);

  // The `RunLoop` in the `TestFuture`s will not spin and process the
  // `CodecFactory` call, until the `TestFuture`s are resolved via `Get()`.
  EXPECT_TRUE(media::IsVideoDecoderConfigSupported(
      future_1.Get(), kUnencryptedH264Base1080Config));
  EXPECT_TRUE(media::IsVideoDecoderConfigSupported(
      future_2.Get(), kUnencryptedH264Base1080Config));
  EXPECT_EQ(codec_factory.GetRequestedCount(), 1);
}

}  // namespace content
