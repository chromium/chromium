// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_factory.h"

#include <array>
#include <memory>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_decoder_adapter.h"
#include "third_party/webrtc/media/base/h264_profile_level_id.h"
#include "third_party/webrtc/media/base/media_constants.h"
#include "third_party/webrtc/media/base/vp9_profile.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
namespace {

const int kDefaultFps = 30;
// Any reasonable size, will be overridden by the decoder anyway.
const gfx::Size kDefaultSize(640, 480);

const base::Feature kRtcDecoderSupportTimeout{"RtcDecoderSupportTimeout",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const uint32_t kDefaultRtcDecoderSupportTimeoutMs = 10000;

struct CodecConfig {
  media::VideoCodec codec;
  media::VideoCodecProfile profile;
};

constexpr std::array<CodecConfig, 7> kCodecConfigs = {{
    {media::kCodecVP8, media::VP8PROFILE_ANY},
    {media::kCodecVP9, media::VP9PROFILE_PROFILE0},
    {media::kCodecVP9, media::VP9PROFILE_PROFILE1},
    {media::kCodecVP9, media::VP9PROFILE_PROFILE2},
    {media::kCodecH264, media::H264PROFILE_BASELINE},
    {media::kCodecH264, media::H264PROFILE_MAIN},
    {media::kCodecH264, media::H264PROFILE_HIGH},
}};

// Translate from media::VideoDecoderConfig to webrtc::SdpVideoFormat, or return
// nothing if the profile isn't supported.
base::Optional<webrtc::SdpVideoFormat> VdcToWebRtcFormat(
    const media::VideoDecoderConfig& config) {
  switch (config.codec()) {
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
          return base::nullopt;
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
          return base::nullopt;
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
      return base::nullopt;
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
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
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
  bool PrefersLateDecoding() const override {
    return decoder_->PrefersLateDecoding();
  }
  const char* ImplementationName() const override {
    return decoder_->ImplementationName();
  }

  // Runs on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  ~ScopedVideoDecoder() override {
    task_runner_->DeleteSoon(FROM_HERE, decoder_.release());
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<webrtc::VideoDecoder> decoder_;
};

base::Optional<base::TimeDelta> GetRtcDecoderSupportTimeoutMs() {
  if (!base::FeatureList::IsEnabled(kRtcDecoderSupportTimeout)) {
    return base::nullopt;
  }
  int timeout_ms = base::GetFieldTrialParamByFeatureAsInt(
      kRtcDecoderSupportTimeout, "timeout", kDefaultRtcDecoderSupportTimeoutMs);
  return base::TimeDelta::FromMilliseconds(timeout_ms);
}

}  // namespace

RTCVideoDecoderFactory::RTCVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories),
      decoder_support_known_(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED),
      decoder_support_timeout_ms_(GetRtcDecoderSupportTimeoutMs()) {
  DVLOG(2) << __func__;
}

bool RTCVideoDecoderFactory::IsDecoderSupportKnown() const {
  if (gpu_factories_->IsDecoderSupportKnown()) {
    return true;
  }

  if (!decoder_support_timeout_ms_) {
    return false;
  }

  // Callback passed to NotifyDecoderSupportKnown is called on caller's
  // sequence. To not block the callback while waiting for it, call
  // NotifyDecoderSupportKnown on a separate sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  bool is_decoder_support_notification_requested = task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](media::GpuVideoAcceleratorFactories* gpu_factories,
                        base::WaitableEvent* decoder_support_known) {
                       gpu_factories->NotifyDecoderSupportKnown(base::BindOnce(
                           [](base::WaitableEvent* decoder_support_known) {
                             decoder_support_known->Signal();
                           },
                           decoder_support_known));
                     },
                     gpu_factories_, &decoder_support_known_));

  if (!is_decoder_support_notification_requested) {
    DLOG(WARNING) << "Failed to request decoder support notification.";
    return false;
  }

  return decoder_support_known_.TimedWait(*decoder_support_timeout_ms_);
}

std::vector<webrtc::SdpVideoFormat>
RTCVideoDecoderFactory::GetSupportedFormats() const {
  if (!IsDecoderSupportKnown()) {
    DLOG(WARNING) << "Decoder support is unknown. Timeout "
                  << decoder_support_timeout_ms_.value_or(base::TimeDelta())
                         .InMilliseconds()
                  << "ms. Decoders might not be available.";
  }

  std::vector<webrtc::SdpVideoFormat> supported_formats;
  for (auto& codec_config : kCodecConfigs) {
    media::VideoDecoderConfig config(
        codec_config.codec, codec_config.profile,
        media::VideoDecoderConfig::AlphaMode::kIsOpaque,
        media::VideoColorSpace(), media::kNoTransformation, kDefaultSize,
        gfx::Rect(kDefaultSize), kDefaultSize, media::EmptyExtraData(),
        media::EncryptionScheme::kUnencrypted);
    for (auto impl : RTCVideoDecoderAdapter::SupportedImplementations()) {
      if (gpu_factories_->IsDecoderConfigSupported(impl, config) ==
          media::GpuVideoAcceleratorFactories::Supported::kTrue) {
        base::Optional<webrtc::SdpVideoFormat> format =
            VdcToWebRtcFormat(config);
        if (format) {
          supported_formats.push_back(*format);
        }
        break;
      }
    }
  }
  MapBaselineProfile(&supported_formats);
  return supported_formats;
}

RTCVideoDecoderFactory::~RTCVideoDecoderFactory() {
  DVLOG(2) << __func__;
}

std::unique_ptr<webrtc::VideoDecoder>
RTCVideoDecoderFactory::CreateVideoDecoder(
    const webrtc::SdpVideoFormat& format) {
  DVLOG(2) << __func__;
  if (!IsDecoderSupportKnown()) {
    DLOG(WARNING) << "Decoder support is unknown. Timeout "
                  << decoder_support_timeout_ms_.value_or(base::TimeDelta())
                         .InMilliseconds()
                  << "ms. Decoders might not be available.";
  }

  std::unique_ptr<webrtc::VideoDecoder> decoder =
      RTCVideoDecoderAdapter::Create(gpu_factories_, format);
  // ScopedVideoDecoder uses the task runner to make sure the decoder is
  // destructed on the correct thread.
  return decoder ? std::make_unique<ScopedVideoDecoder>(
                       gpu_factories_->GetTaskRunner(), std::move(decoder))
                 : nullptr;
}

}  // namespace blink
