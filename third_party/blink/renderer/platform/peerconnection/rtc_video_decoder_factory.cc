// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_factory.h"

#include <array>
#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_adapter.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_stream_adapter.h"
#include "third_party/blink/renderer/platform/webrtc/webrtc_video_utils.h"
#include "third_party/webrtc/media/base/h264_profile_level_id.h"
#include "third_party/webrtc/media/base/media_constants.h"
#include "third_party/webrtc/media/base/vp9_profile.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
namespace {

// The default fps and default size are used when querying gpu_factories_ to see
// if a codec profile is supported. 1280x720 at 30 fps corresponds to level 3.1
// for both VP9 and H264. This matches the maximum H264 profile level that is
// returned by the internal software decoder.
// TODO(crbug.com/1213437): Query gpu_factories_ or decoder_factory_ to
// determine the maximum resolution and frame rate.
const int kDefaultFps = 30;
const gfx::Size kDefaultSize(1280, 720);

struct CodecConfig {
  media::VideoCodec codec;
  media::VideoCodecProfile profile;
};

constexpr std::array<CodecConfig, 8> kCodecConfigs = {{
    {media::kCodecVP8, media::VP8PROFILE_ANY},
    {media::kCodecVP9, media::VP9PROFILE_PROFILE0},
    {media::kCodecVP9, media::VP9PROFILE_PROFILE1},
    {media::kCodecVP9, media::VP9PROFILE_PROFILE2},
    {media::kCodecH264, media::H264PROFILE_BASELINE},
    {media::kCodecH264, media::H264PROFILE_MAIN},
    {media::kCodecH264, media::H264PROFILE_HIGH},
    {media::kCodecAV1, media::AV1PROFILE_PROFILE_MAIN},
}};

// Translate from media::VideoDecoderConfig to webrtc::SdpVideoFormat, or return
// nothing if the profile isn't supported.
absl::optional<webrtc::SdpVideoFormat> VdcToWebRtcFormat(
    const media::VideoDecoderConfig& config) {
  switch (config.codec()) {
    case media::VideoCodec::kCodecAV1:
      return webrtc::SdpVideoFormat("AV1X");
    case media::VideoCodec::kCodecVP8:
      return webrtc::SdpVideoFormat("VP8");
    case media::VideoCodec::kCodecVP9: {
      webrtc::VP9Profile vp9_profile;
      switch (config.profile()) {
        case media::VP9PROFILE_PROFILE0:
          vp9_profile = webrtc::VP9Profile::kProfile0;
          break;
        case media::VP9PROFILE_PROFILE1:
          vp9_profile = webrtc::VP9Profile::kProfile1;
          break;
        case media::VP9PROFILE_PROFILE2:
          vp9_profile = webrtc::VP9Profile::kProfile2;
          break;
        default:
          // Unsupported profile in WebRTC.
          return absl::nullopt;
      }
      return webrtc::SdpVideoFormat(
          "VP9", {{webrtc::kVP9FmtpProfileId,
                   webrtc::VP9ProfileToString(vp9_profile)}});
    }
    case media::VideoCodec::kCodecH264: {
      webrtc::H264::Profile h264_profile;
      switch (config.profile()) {
        case media::H264PROFILE_BASELINE:
          h264_profile = webrtc::H264::kProfileBaseline;
          break;
        case media::H264PROFILE_MAIN:
          h264_profile = webrtc::H264::kProfileMain;
          break;
        case media::H264PROFILE_HIGH:
          h264_profile = webrtc::H264::kProfileHigh;
          break;
        default:
          // Unsupported H264 profile in WebRTC.
          return absl::nullopt;
      }

      const int width = config.visible_rect().width();
      const int height = config.visible_rect().height();

      const absl::optional<webrtc::H264::Level> h264_level =
          webrtc::H264::SupportedLevel(width * height, kDefaultFps);
      const webrtc::H264::ProfileLevelId profile_level_id(
          h264_profile, h264_level.value_or(webrtc::H264::kLevel1));

      webrtc::SdpVideoFormat format("H264");
      format.parameters = {
          {cricket::kH264FmtpProfileLevelId,
           *webrtc::H264::ProfileLevelIdToString(profile_level_id)},
          {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
          {cricket::kH264FmtpPacketizationMode, "1"}};
      return format;
    }
    default:
      return absl::nullopt;
  }
}

// Due to https://crbug.com/345569, HW decoders do not distinguish between
// Constrained Baseline(CBP) and Baseline(BP) profiles. Since CBP is a subset of
// BP, we can report support for both. It is safe to do so when SW fallback is
// available.
// TODO(emircan): Remove this when the bug referred above is fixed.
void MapBaselineProfile(
    std::vector<webrtc::SdpVideoFormat>* supported_formats) {
  for (const auto& format : *supported_formats) {
    const absl::optional<webrtc::H264::ProfileLevelId> profile_level_id =
        webrtc::H264::ParseSdpProfileLevelId(format.parameters);
    if (profile_level_id &&
        profile_level_id->profile == webrtc::H264::kProfileBaseline) {
      webrtc::SdpVideoFormat cbp_format = format;
      webrtc::H264::ProfileLevelId cbp_profile = *profile_level_id;
      cbp_profile.profile = webrtc::H264::kProfileConstrainedBaseline;
      cbp_format.parameters[cricket::kH264FmtpProfileLevelId] =
          *webrtc::H264::ProfileLevelIdToString(cbp_profile);
      supported_formats->push_back(cbp_format);
      return;
    }
  }
}

// This extra indirection is needed so that we can delete the decoder on the
// correct thread.
class ScopedVideoDecoder : public webrtc::VideoDecoder {
 public:
  ScopedVideoDecoder(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      std::unique_ptr<webrtc::VideoDecoder> decoder)
      : task_runner_(task_runner), decoder_(std::move(decoder)) {}

  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override {
    return decoder_->InitDecode(codec_settings, number_of_cores);
  }
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override {
    return decoder_->RegisterDecodeCompleteCallback(callback);
  }
  int32_t Release() override { return decoder_->Release(); }
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 int64_t render_time_ms) override {
    return decoder_->Decode(input_image, missing_frames, render_time_ms);
  }

  DecoderInfo GetDecoderInfo() const override {
    return decoder_->GetDecoderInfo();
  }

  // Runs on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  ~ScopedVideoDecoder() override {
    task_runner_->DeleteSoon(FROM_HERE, decoder_.release());
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<webrtc::VideoDecoder> decoder_;
};

}  // namespace

RTCVideoDecoderFactory::RTCVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::DecoderFactory* decoder_factory,
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    const gfx::ColorSpace& render_color_space)
    : gpu_factories_(gpu_factories),
      decoder_factory_(decoder_factory),
      media_task_runner_(std::move(media_task_runner)),
      render_color_space_(render_color_space) {
  if (gpu_factories_) {
    gpu_codec_support_waiter_ =
        std::make_unique<GpuCodecSupportWaiter>(gpu_factories_);
  }
  DVLOG(2) << __func__;
}

void RTCVideoDecoderFactory::CheckAndWaitDecoderSupportStatusIfNeeded() const {
  if (!gpu_codec_support_waiter_)
    return;

  if (!gpu_codec_support_waiter_->IsDecoderSupportKnown()) {
    DLOG(WARNING) << "Decoder support is unknown. Timeout "
                  << gpu_codec_support_waiter_->wait_timeout_ms()
                         .value_or(base::TimeDelta())
                         .InMilliseconds()
                  << "ms. Decoders might not be available.";
  }
}

std::vector<webrtc::SdpVideoFormat>
RTCVideoDecoderFactory::GetSupportedFormats() const {
  CheckAndWaitDecoderSupportStatusIfNeeded();

  media::SupportedVideoDecoderConfigs supported_decoder_factory_configs =
      decoder_factory_->GetSupportedVideoDecoderConfigsForWebRTC();

  // For now, ignore `kUseDecoderStreamForWebRTC`, and advertise support only
  // for hardware-accelerated formats.  For some codecs, like AV1, which don't
  // have an equivalent in rtc, we might want to include them anyway.
  std::vector<webrtc::SdpVideoFormat> supported_formats;
  for (auto& codec_config : kCodecConfigs) {
    media::VideoDecoderConfig config(
        codec_config.codec, codec_config.profile,
        media::VideoDecoderConfig::AlphaMode::kIsOpaque,
        media::VideoColorSpace(), media::kNoTransformation, kDefaultSize,
        gfx::Rect(kDefaultSize), kDefaultSize, media::EmptyExtraData(),
        media::EncryptionScheme::kUnencrypted);
    absl::optional<webrtc::SdpVideoFormat> format;

    // The RTCVideoDecoderAdapter is for HW decoders only, so ignore it if there
    // are no gpu_factories_.
    if (gpu_factories_ &&
        gpu_factories_->IsDecoderConfigSupported(config) ==
            media::GpuVideoAcceleratorFactories::Supported::kTrue) {
      format = VdcToWebRtcFormat(config);
    }

    if (base::FeatureList::IsEnabled(media::kUseDecoderStreamForWebRTC) &&
        !format.has_value()) {
      for (auto& supported_config : supported_decoder_factory_configs) {
        if (supported_config.Matches(config)) {
          format = VdcToWebRtcFormat(config);
          break;
        }
      }
    }

    if (format)
      supported_formats.push_back(*format);
  }

  MapBaselineProfile(&supported_formats);
  return supported_formats;
}

webrtc::VideoDecoderFactory::CodecSupport
RTCVideoDecoderFactory::QueryCodecSupport(
    const webrtc::SdpVideoFormat& format,
    absl::optional<std::string> scalability_mode) const {
  media::VideoCodec codec =
      WebRtcToMediaVideoCodec(webrtc::PayloadStringToCodecType(format.name));
  if (scalability_mode) {
    absl::optional<int> spatial_layers =
        WebRtcScalabilityModeSpatialLayers(*scalability_mode);

    // Check that the scalability mode was correctly parsed and that the
    // configuration is valid (e.g., H264 doesn't support SVC at all and VP8
    // doesn't support spatial layers).
    if (!spatial_layers ||
        (codec != media::kCodecVP8 && codec != media::kCodecVP9 &&
         codec != media::kCodecAV1) ||
        (codec == media::kCodecVP8 && *spatial_layers > 1)) {
      // Ivalid scalability_mode, return unsupported.
      return {false, false};
    }
    DCHECK(spatial_layers);
    // Most HW decoders cannot handle spatial layers, so return false if the
    // configuration contains spatial layers unless we explicitly know that the
    // HW decoder can handle spatial layers.
    if (codec == media::kCodecVP9 && *spatial_layers > 1 &&
        !RTCVideoDecoderAdapter::Vp9HwSupportForSpatialLayers()) {
      return {false, false};
    }
  }

  CheckAndWaitDecoderSupportStatusIfNeeded();

  media::VideoCodecProfile codec_profile =
      WebRtcVideoFormatToMediaVideoCodecProfile(format);
  media::VideoDecoderConfig config(
      codec, codec_profile, media::VideoDecoderConfig::AlphaMode::kIsOpaque,
      media::VideoColorSpace(), media::kNoTransformation, kDefaultSize,
      gfx::Rect(kDefaultSize), kDefaultSize, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted);

  webrtc::VideoDecoderFactory::CodecSupport codec_support;
  // Check gpu_factories for powerEfficient.
  if (gpu_factories_) {
    if (gpu_factories_->IsDecoderConfigSupported(config) ==
        media::GpuVideoAcceleratorFactories::Supported::kTrue) {
      codec_support.is_power_efficient = true;
    }
  }

  // The codec must be supported if it's power efficient.
  codec_support.is_supported = codec_support.is_power_efficient;

  // RtcDecoderStreamAdapter supports all codecs with HW support and potentially
  // a few codecs in SW.
  if (!codec_support.is_supported &&
      base::FeatureList::IsEnabled(media::kUseDecoderStreamForWebRTC)) {
    media::SupportedVideoDecoderConfigs supported_decoder_factory_configs =
        decoder_factory_->GetSupportedVideoDecoderConfigsForWebRTC();
    for (auto& supported_config : supported_decoder_factory_configs) {
      if (supported_config.Matches(config)) {
        codec_support.is_supported = true;
        break;
      }
    }
  }

  return codec_support;
}

RTCVideoDecoderFactory::~RTCVideoDecoderFactory() {
  DVLOG(2) << __func__;
}

std::unique_ptr<webrtc::VideoDecoder>
RTCVideoDecoderFactory::CreateVideoDecoder(
    const webrtc::SdpVideoFormat& format) {
  TRACE_EVENT0("webrtc", "RTCVideoDecoderFactory::CreateVideoDecoder");
  DVLOG(2) << __func__;
  CheckAndWaitDecoderSupportStatusIfNeeded();

  std::unique_ptr<webrtc::VideoDecoder> decoder;
  if (base::FeatureList::IsEnabled(media::kUseDecoderStreamForWebRTC)) {
    decoder = RTCVideoDecoderStreamAdapter::Create(
        gpu_factories_, decoder_factory_, media_task_runner_,
        render_color_space_, format);
  } else {
    decoder = RTCVideoDecoderAdapter::Create(gpu_factories_, format);
  }
  // ScopedVideoDecoder uses the task runner to make sure the decoder is
  // destructed on the correct thread.
  return decoder ? std::make_unique<ScopedVideoDecoder>(media_task_runner_,
                                                        std::move(decoder))
                 : nullptr;
}

}  // namespace blink
