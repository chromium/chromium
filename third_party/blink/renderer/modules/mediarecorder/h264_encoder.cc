// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediarecorder/h264_encoder.h"

#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/openh264/src/codec/api/wels/codec_app_def.h"
#include "third_party/openh264/src/codec/api/wels/codec_def.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
namespace {

absl::optional<EProfileIdc> ToOpenH264Profile(
    media::VideoCodecProfile profile) {
  static constexpr auto kProfileToEProfileIdc =
      base::MakeFixedFlatMap<media::VideoCodecProfile, EProfileIdc>({
          {media::H264PROFILE_BASELINE, PRO_BASELINE},
          {media::H264PROFILE_MAIN, PRO_MAIN},
          {media::H264PROFILE_EXTENDED, PRO_EXTENDED},
          {media::H264PROFILE_HIGH, PRO_HIGH},
      });

  const auto* it = kProfileToEProfileIdc.find(profile);
  if (it != kProfileToEProfileIdc.end()) {
    return it->second;
  }
  return absl::nullopt;
}

absl::optional<ELevelIdc> ToOpenH264Level(uint8_t level) {
  static constexpr auto kLevelToELevelIdc =
      base::MakeFixedFlatMap<uint8_t, ELevelIdc>({
          {10, LEVEL_1_0},
          {9, LEVEL_1_B},
          {11, LEVEL_1_1},
          {12, LEVEL_1_2},
          {13, LEVEL_1_3},
          {20, LEVEL_2_0},
          {21, LEVEL_2_1},
          {22, LEVEL_2_2},
          {30, LEVEL_3_0},
          {31, LEVEL_3_1},
          {32, LEVEL_3_2},
          {40, LEVEL_4_0},
          {41, LEVEL_4_1},
          {42, LEVEL_4_2},
          {50, LEVEL_5_0},
          {51, LEVEL_5_1},
          {52, LEVEL_5_2},
      });

  const auto* it = kLevelToELevelIdc.find(level);
  if (it != kLevelToELevelIdc.end())
    return it->second;
  return absl::nullopt;
}
}  // namespace

void H264Encoder::ISVCEncoderDeleter::operator()(ISVCEncoder* codec) {
  if (!codec)
    return;
  const int uninit_ret = codec->Uninitialize();
  CHECK_EQ(cmResultSuccess, uninit_ret);
  WelsDestroySVCEncoder(codec);
}

H264Encoder::H264Encoder(
    scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
    VideoTrackRecorder::CodecProfile codec_profile,
    uint32_t bits_per_second)
    : Encoder(std::move(encoding_task_runner),
              on_encoded_video_cb,
              bits_per_second),
      codec_profile_(codec_profile) {
  DCHECK_EQ(codec_profile_.codec_id, VideoTrackRecorder::CodecId::kH264);
}

// Needs to be defined here to combat a Windows linking issue.
H264Encoder::~H264Encoder() = default;

void H264Encoder::EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                              base::TimeTicks capture_timestamp,
                              bool request_keyframe) {
  TRACE_EVENT0("media", "H264Encoder::EncodeFrame");
  using media::VideoFrame;
  DCHECK(frame->format() == media::VideoPixelFormat::PIXEL_FORMAT_NV12 ||
         frame->format() == media::VideoPixelFormat::PIXEL_FORMAT_I420 ||
         frame->format() == media::VideoPixelFormat::PIXEL_FORMAT_I420A);

  if (frame->format() == media::PIXEL_FORMAT_NV12) {
    frame = ConvertToI420ForSoftwareEncoder(frame);
    if (!frame) {
      DLOG(ERROR) << "VideoFrame failed to map";
      return;
    }
  }
  DCHECK(frame->IsMappable());

  const gfx::Size frame_size = frame->visible_rect().size();
  if (!openh264_encoder_ || configured_size_ != frame_size) {
    if (!ConfigureEncoder(frame_size)) {
      return;
    }
    first_frame_timestamp_ = capture_timestamp;
  }

  SSourcePicture picture = {};
  picture.iPicWidth = frame_size.width();
  picture.iPicHeight = frame_size.height();
  picture.iColorFormat = EVideoFormatType::videoFormatI420;
  picture.uiTimeStamp =
      (capture_timestamp - first_frame_timestamp_).InMilliseconds();
  picture.iStride[0] = frame->stride(VideoFrame::kYPlane);
  picture.iStride[1] = frame->stride(VideoFrame::kUPlane);
  picture.iStride[2] = frame->stride(VideoFrame::kVPlane);
  picture.pData[0] =
      const_cast<uint8_t*>(frame->visible_data(VideoFrame::kYPlane));
  picture.pData[1] =
      const_cast<uint8_t*>(frame->visible_data(VideoFrame::kUPlane));
  picture.pData[2] =
      const_cast<uint8_t*>(frame->visible_data(VideoFrame::kVPlane));

  SFrameBSInfo info = {};

  // ForceIntraFrame(false) should be nop, but actually logs, avoid this.
  if (request_keyframe) {
    openh264_encoder_->ForceIntraFrame(true);
  }
  if (openh264_encoder_->EncodeFrame(&picture, &info) != cmResultSuccess) {
    NOTREACHED() << "OpenH264 encoding failed";
    return;
  }
  const media::Muxer::VideoParameters video_params(*frame);
  frame = nullptr;

  std::string data;
  const uint8_t kNALStartCode[4] = {0, 0, 0, 1};
  for (int layer = 0; layer < info.iLayerNum; ++layer) {
    const SLayerBSInfo& layerInfo = info.sLayerInfo[layer];
    // Iterate NAL units making up this layer, noting fragments.
    size_t layer_len = 0;
    for (int nal = 0; nal < layerInfo.iNalCount; ++nal) {
      // The following DCHECKs make sure that the header of each NAL unit is OK.
      DCHECK_GE(layerInfo.pNalLengthInByte[nal], 4);
      DCHECK_EQ(kNALStartCode[0], layerInfo.pBsBuf[layer_len + 0]);
      DCHECK_EQ(kNALStartCode[1], layerInfo.pBsBuf[layer_len + 1]);
      DCHECK_EQ(kNALStartCode[2], layerInfo.pBsBuf[layer_len + 2]);
      DCHECK_EQ(kNALStartCode[3], layerInfo.pBsBuf[layer_len + 3]);

      layer_len += layerInfo.pNalLengthInByte[nal];
    }
    // Copy the entire layer's data (including NAL start codes).
    data.append(reinterpret_cast<char*>(layerInfo.pBsBuf), layer_len);
  }

  const bool is_key_frame = info.eFrameType == videoFrameTypeIDR;
  on_encoded_video_cb_.Run(video_params, std::move(data), std::string(),
                           capture_timestamp, is_key_frame);
}

bool H264Encoder::ConfigureEncoder(const gfx::Size& size) {
  TRACE_EVENT0("media", "H264Encoder::ConfigureEncoder");
  ISVCEncoder* temp_encoder = nullptr;
  if (WelsCreateSVCEncoder(&temp_encoder) != 0) {
    NOTREACHED() << "Failed to create OpenH264 encoder";
    return false;
  }
  openh264_encoder_.reset(temp_encoder);
  configured_size_ = size;

#if DCHECK_IS_ON()
  int trace_level = WELS_LOG_INFO;
  openh264_encoder_->SetOption(ENCODER_OPTION_TRACE_LEVEL, &trace_level);
#endif

  SEncParamExt init_params;
  openh264_encoder_->GetDefaultParams(&init_params);
  init_params.iUsageType = CAMERA_VIDEO_REAL_TIME;

  DCHECK_EQ(AUTO_REF_PIC_COUNT, init_params.iNumRefFrame);
  DCHECK(!init_params.bSimulcastAVC);

  init_params.iPicWidth = size.width();
  init_params.iPicHeight = size.height();

  DCHECK_EQ(RC_QUALITY_MODE, init_params.iRCMode);
  DCHECK_EQ(0, init_params.iPaddingFlag);
  DCHECK_EQ(UNSPECIFIED_BIT_RATE, init_params.iTargetBitrate);
  DCHECK_EQ(UNSPECIFIED_BIT_RATE, init_params.iMaxBitrate);
  if (bits_per_second_ > 0) {
    init_params.iRCMode = RC_BITRATE_MODE;
    init_params.iTargetBitrate = bits_per_second_;
  } else {
    init_params.iRCMode = RC_OFF_MODE;
  }

#if BUILDFLAG(IS_CHROMEOS)
  init_params.iMultipleThreadIdc = 0;
#else
  // Threading model: Set to 1 due to https://crbug.com/583348.
  init_params.iMultipleThreadIdc = 1;
#endif

  // TODO(mcasas): consider reducing complexity if there are few CPUs available.
  init_params.iComplexityMode = MEDIUM_COMPLEXITY;
  DCHECK(!init_params.bEnableDenoise);
  DCHECK(init_params.bEnableFrameSkip);

  // The base spatial layer 0 is the only one we use.
  DCHECK_EQ(1, init_params.iSpatialLayerNum);
  init_params.sSpatialLayers[0].iVideoWidth = init_params.iPicWidth;
  init_params.sSpatialLayers[0].iVideoHeight = init_params.iPicHeight;
  init_params.sSpatialLayers[0].iSpatialBitrate = init_params.iTargetBitrate;

  // Input profile may be optional, fills PRO_UNKNOWN for auto-detection.
  init_params.sSpatialLayers[0].uiProfileIdc =
      codec_profile_.profile
          ? ToOpenH264Profile(*codec_profile_.profile).value_or(PRO_UNKNOWN)
          : PRO_UNKNOWN;
  // Input level may be optional, fills LEVEL_UNKNOWN for auto-detection.
  init_params.sSpatialLayers[0].uiLevelIdc =
      codec_profile_.level
          ? ToOpenH264Level(*codec_profile_.level).value_or(LEVEL_UNKNOWN)
          : LEVEL_UNKNOWN;
  DCHECK_EQ(init_params.sSpatialLayers[0].uiProfileIdc == PRO_UNKNOWN,
            init_params.sSpatialLayers[0].uiLevelIdc == LEVEL_UNKNOWN);

  // When uiSliceMode = SM_FIXEDSLCNUM_SLICE, uiSliceNum = 0 means auto design
  // it with cpu core number.
  init_params.sSpatialLayers[0].sSliceArgument.uiSliceNum = 0;
  init_params.sSpatialLayers[0].sSliceArgument.uiSliceMode =
      SM_FIXEDSLCNUM_SLICE;

  if (openh264_encoder_->InitializeExt(&init_params) != cmResultSuccess) {
    DLOG(WARNING) << "Failed to initialize OpenH264 encoder";
    openh264_encoder_.reset();
    return false;
  }

  int pixel_format = EVideoFormatType::videoFormatI420;
  openh264_encoder_->SetOption(ENCODER_OPTION_DATAFORMAT, &pixel_format);
  return true;
}

SEncParamExt H264Encoder::GetEncoderOptionForTesting() {
  DCHECK(openh264_encoder_)
      << "Call GetOption on uninitialized OpenH264 encoder";

  SEncParamExt params;
  if (openh264_encoder_->GetOption(ENCODER_OPTION_SVC_ENCODE_PARAM_EXT,
                                   &params) != 0) {
    NOTREACHED() << "Failed to get ENCODER_OPTION_SVC_ENCODE_PARAM_EXT";
  }

  return params;
}

}  // namespace blink
