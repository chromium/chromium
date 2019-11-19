// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_dummy_video_encoder.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "remoting/protocol/video_channel_state_observer.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

#if defined(USE_H264_ENCODER)
#include "media/video/h264_parser.h"
#endif

namespace remoting {
namespace protocol {

#if defined(USE_H264_ENCODER)
namespace {

// Populates struct webrtc::RTPFragmentationHeader for H264 codec.
// Each entry specifies the offset and length (excluding start code) of a NALU.
// Returns true if successful.
bool GetRTPFragmentationHeaderH264(webrtc::RTPFragmentationHeader* header,
                                   const uint8_t* data, uint32_t length) {
  std::vector<media::H264NALU> nalu_vector;
  if (!media::H264Parser::ParseNALUs(data, length, &nalu_vector)) {
    // H264Parser::ParseNALUs() has logged the errors already.
    return false;
  }

  // TODO(zijiehe): Find a right place to share the following logic between
  // //content and //remoting.
  header->VerifyAndAllocateFragmentationHeader(nalu_vector.size());
  for (size_t i = 0; i < nalu_vector.size(); ++i) {
    header->fragmentationOffset[i] = nalu_vector[i].data - data;
    header->fragmentationLength[i] = nalu_vector[i].size;
  }
  return true;
}

}  // namespace
#endif

WebrtcDummyVideoEncoder::WebrtcDummyVideoEncoder(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer,
    WebrtcDummyVideoEncoderFactory* factory)
    : main_task_runner_(main_task_runner),
      state_(kUninitialized),
      video_channel_state_observer_(video_channel_state_observer),
      factory_(factory) {}

WebrtcDummyVideoEncoder::~WebrtcDummyVideoEncoder() {
  if (factory_) {
    factory_->EncoderDestroyed(this);
  }
}

int32_t WebrtcDummyVideoEncoder::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    int32_t number_of_cores,
    size_t max_payload_size) {
  DCHECK(codec_settings);
  base::AutoLock lock(lock_);
  int stream_count = codec_settings->numberOfSimulcastStreams;
  // Validate request is to support a single stream.
  if (stream_count > 1) {
    for (int i = 0; i < stream_count; ++i) {
      if (codec_settings->simulcastStream[i].maxBitrate != 0) {
        LOG(ERROR) << "Simulcast unsupported";
        return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
      }
    }
  }
  state_ = kInitialized;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDummyVideoEncoder::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  base::AutoLock lock(lock_);
  encoded_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDummyVideoEncoder::Release() {
  base::AutoLock lock(lock_);
  encoded_callback_ = nullptr;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t WebrtcDummyVideoEncoder::Encode(
    const webrtc::VideoFrame& frame,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  // WebrtcDummyVideoCapturer doesn't generate any video frames, so Encode() can
  // be called only from VCMGenericEncoder::RequestFrame() to request a key
  // frame.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoChannelStateObserver::OnKeyFrameRequested,
                                video_channel_state_observer_));
  return WEBRTC_VIDEO_CODEC_OK;
}

void WebrtcDummyVideoEncoder::SetRates(
    const RateControlParameters& parameters) {
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoChannelStateObserver::OnTargetBitrateChanged,
                     video_channel_state_observer_,
                     parameters.bitrate.get_sum_kbps()));
  // framerate is not expected to be valid given we never report captured
  // frames.
}

webrtc::EncodedImageCallback::Result WebrtcDummyVideoEncoder::SendEncodedFrame(
    const WebrtcVideoEncoder::EncodedFrame& frame,
    base::TimeTicks capture_time,
    base::TimeTicks encode_started_time,
    base::TimeTicks encode_finished_time) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  const uint8_t* buffer =
      reinterpret_cast<const uint8_t*>(base::data(frame.data));
  size_t buffer_size = frame.data.size();
  base::AutoLock lock(lock_);
  if (state_ == kUninitialized) {
    LOG(ERROR) << "encoder interface uninitialized";
    return webrtc::EncodedImageCallback::Result(
        webrtc::EncodedImageCallback::Result::ERROR_SEND_FAILED);
  }

  webrtc::EncodedImage encoded_image;
  encoded_image.SetEncodedData(
      webrtc::EncodedImageBuffer::Create(buffer, buffer_size));
  encoded_image._encodedWidth = frame.size.width();
  encoded_image._encodedHeight = frame.size.height();
  encoded_image._completeFrame = true;
  encoded_image._frameType = frame.key_frame
                                 ? webrtc::VideoFrameType::kVideoFrameKey
                                 : webrtc::VideoFrameType::kVideoFrameDelta;
  int64_t capture_time_ms = (capture_time - base::TimeTicks()).InMilliseconds();
  int64_t encode_started_time_ms =
      (encode_started_time - base::TimeTicks()).InMilliseconds();
  int64_t encode_finished_time_ms =
      (encode_finished_time - base::TimeTicks()).InMilliseconds();
  encoded_image.capture_time_ms_ = capture_time_ms;
  encoded_image.SetTimestamp(static_cast<uint32_t>(capture_time_ms * 90));
  encoded_image.playout_delay_.min_ms = 0;
  encoded_image.playout_delay_.max_ms = 0;
  encoded_image.timing_.encode_start_ms = encode_started_time_ms;
  encoded_image.timing_.encode_finish_ms = encode_finished_time_ms;
  encoded_image.content_type_ = webrtc::VideoContentType::SCREENSHARE;

  webrtc::CodecSpecificInfo codec_specific_info;
  codec_specific_info.codecType = frame.codec;

  if (frame.codec == webrtc::kVideoCodecVP8) {
    webrtc::CodecSpecificInfoVP8* vp8_info =
        &codec_specific_info.codecSpecific.VP8;
    vp8_info->temporalIdx = webrtc::kNoTemporalIdx;
  } else if (frame.codec == webrtc::kVideoCodecVP9) {
    webrtc::CodecSpecificInfoVP9* vp9_info =
        &codec_specific_info.codecSpecific.VP9;
    vp9_info->inter_pic_predicted = !frame.key_frame;
    vp9_info->ss_data_available = frame.key_frame;
    vp9_info->spatial_layer_resolution_present = frame.key_frame;
    if (frame.key_frame) {
      vp9_info->width[0] = frame.size.width();
      vp9_info->height[0] = frame.size.height();
    }
    vp9_info->num_spatial_layers = 1;
    vp9_info->gof_idx = webrtc::kNoGofIdx;
    vp9_info->temporal_idx = webrtc::kNoTemporalIdx;
    vp9_info->flexible_mode = false;
    vp9_info->temporal_up_switch = true;
    vp9_info->inter_layer_predicted = false;
    vp9_info->first_frame_in_picture = true;
    vp9_info->end_of_picture = true;
    vp9_info->spatial_layer_resolution_present = false;
  } else if (frame.codec == webrtc::kVideoCodecH264) {
#if defined(USE_H264_ENCODER)
    webrtc::CodecSpecificInfoH264* h264_info =
        &codec_specific_info.codecSpecific.H264;
    h264_info->packetization_mode =
        webrtc::H264PacketizationMode::NonInterleaved;
#else
    NOTREACHED();
#endif
  } else {
    NOTREACHED();
  }

  webrtc::RTPFragmentationHeader header;
  if (frame.codec == webrtc::kVideoCodecH264) {
#if defined(USE_H264_ENCODER)
    if (!GetRTPFragmentationHeaderH264(&header, buffer, buffer_size)) {
      return webrtc::EncodedImageCallback::Result(
          webrtc::EncodedImageCallback::Result::ERROR_SEND_FAILED);
    }
#else
    NOTREACHED();
#endif
  } else {
    header.VerifyAndAllocateFragmentationHeader(1);
    header.fragmentationOffset[0] = 0;
    header.fragmentationLength[0] = buffer_size;
  }

  DCHECK(encoded_callback_);
  return encoded_callback_->OnEncodedImage(encoded_image, &codec_specific_info,
                                           &header);
}

webrtc::VideoEncoder::EncoderInfo WebrtcDummyVideoEncoder::GetEncoderInfo()
    const {
  EncoderInfo info;
  // TODO(mirtad): Set this flag correctly per encoder.
  info.is_hardware_accelerated = true;
  // Set internal source to true to directly provide encoded frames to webrtc.
  info.has_internal_source = true;
  return info;
}

WebrtcDummyVideoEncoderFactory::WebrtcDummyVideoEncoderFactory()
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  formats_.push_back(webrtc::SdpVideoFormat("VP8"));
  formats_.push_back(webrtc::SdpVideoFormat("VP9"));
  formats_.push_back(webrtc::SdpVideoFormat("H264"));
}

WebrtcDummyVideoEncoderFactory::~WebrtcDummyVideoEncoderFactory() {
  DCHECK(encoders_.empty());
}

std::unique_ptr<webrtc::VideoEncoder>
WebrtcDummyVideoEncoderFactory::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format) {
  webrtc::VideoCodecType type = webrtc::PayloadStringToCodecType(format.name);
  std::unique_ptr<WebrtcDummyVideoEncoder> encoder =
      base::WrapUnique(new WebrtcDummyVideoEncoder(
          main_task_runner_, video_channel_state_observer_, this));
  base::AutoLock lock(lock_);
  encoders_.push_back(encoder.get());
  if (encoder_created_callback_) {
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(encoder_created_callback_, type));
  }
  return encoder;
}

std::vector<webrtc::SdpVideoFormat>
WebrtcDummyVideoEncoderFactory::GetSupportedFormats() const {
  return formats_;
}

WebrtcDummyVideoEncoderFactory::CodecInfo
WebrtcDummyVideoEncoderFactory::QueryVideoEncoder(
    const webrtc::SdpVideoFormat& format) const {
  CodecInfo codec_info;
  codec_info.is_hardware_accelerated = true;
  // Set internal source to true to directly provide encoded frames to webrtc.
  codec_info.has_internal_source = true;
  return codec_info;
}

webrtc::EncodedImageCallback::Result
WebrtcDummyVideoEncoderFactory::SendEncodedFrame(
    const WebrtcVideoEncoder::EncodedFrame& frame,
    base::TimeTicks capture_time,
    base::TimeTicks encode_started_time,
    base::TimeTicks encode_finished_time) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  if (encoders_.size() != 1) {
    LOG(ERROR) << "Unexpected number of encoders " << encoders_.size();
    return webrtc::EncodedImageCallback::Result(
        webrtc::EncodedImageCallback::Result::ERROR_SEND_FAILED);
  }
  return encoders_.front()->SendEncodedFrame(
      frame, capture_time, encode_started_time, encode_finished_time);
}

void WebrtcDummyVideoEncoderFactory::RegisterEncoderSelectedCallback(
    const base::Callback<void(webrtc::VideoCodecType)>& callback) {
  encoder_created_callback_ = callback;
}

void WebrtcDummyVideoEncoderFactory::SetVideoChannelStateObserver(
    base::WeakPtr<VideoChannelStateObserver> video_channel_state_observer) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(encoders_.empty());
  base::AutoLock lock(lock_);
  video_channel_state_observer_ = video_channel_state_observer;
}

void WebrtcDummyVideoEncoderFactory::EncoderDestroyed(
    WebrtcDummyVideoEncoder* encoder) {
  base::AutoLock lock(lock_);
  if (!encoder) {
    LOG(ERROR) << "Attempting to destroy null encoder";
    return;
  }
  for (auto pos = encoders_.begin(); pos != encoders_.end(); ++pos) {
    if (*pos == encoder) {
      encoders_.erase(pos);
      return;
    }
  }
  NOTREACHED() << "Asked to remove encoder not owned by factory.";
}

}  // namespace protocol
}  // namespace remoting
