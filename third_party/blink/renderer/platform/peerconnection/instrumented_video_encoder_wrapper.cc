// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/instrumented_video_encoder_wrapper.h"

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "third_party/webrtc/modules/video_coding/include/video_error_codes.h"

namespace blink {

InstrumentedVideoEncoderWrapper::InstrumentedVideoEncoderWrapper(
    int id,
    std::unique_ptr<webrtc::VideoEncoder> wrapped_encoder,
    VideoEncoderStateObserver* state_observer)
    : id_(id),
      state_observer_(state_observer),
      encoder_sequence_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      wrapped_encoder_(std::move(wrapped_encoder)),
      callback_(nullptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
}

InstrumentedVideoEncoderWrapper::~InstrumentedVideoEncoderWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  wrapped_encoder_->RegisterEncodeCompleteCallback(nullptr);
  weak_this_factory_.InvalidateWeakPtrs();
}

int InstrumentedVideoEncoderWrapper::InitEncode(
    const webrtc::VideoCodec* codec_settings,
    const webrtc::VideoEncoder::Settings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  const int status = wrapped_encoder_->InitEncode(codec_settings, settings);
  if (status == WEBRTC_VIDEO_CODEC_OK) {
    state_observer_->OnEncoderCreated(id_, *codec_settings);
    CHECK_EQ(wrapped_encoder_->RegisterEncodeCompleteCallback(this),
             WEBRTC_VIDEO_CODEC_OK);
  }
  return status;
}

int32_t InstrumentedVideoEncoderWrapper::RegisterEncodeCompleteCallback(
    webrtc::EncodedImageCallback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t InstrumentedVideoEncoderWrapper::Release() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  state_observer_->OnEncoderDestroyed(id_);
  const int status = wrapped_encoder_->Release();
  wrapped_encoder_->RegisterEncodeCompleteCallback(nullptr);
  return status;
}

int32_t InstrumentedVideoEncoderWrapper::Encode(
    const webrtc::VideoFrame& frame,
    const std::vector<webrtc::VideoFrameType>* frame_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  state_observer_->OnEncode(id_, frame.rtp_timestamp());
  return wrapped_encoder_->Encode(frame, frame_types);
}

void InstrumentedVideoEncoderWrapper::SetRates(
    const RateControlParameters& parameters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  wrapped_encoder_->SetRates(parameters);
  Vector<bool> active_layers;
  for (wtf_size_t i = 0; i < webrtc::kMaxSpatialLayers; ++i) {
    if (parameters.bitrate.IsSpatialLayerUsed(i)) {
      while (active_layers.size() + 1 < i) {
        // Backfill in case some lower layers were not used.
        active_layers.push_back(false);
      }
      active_layers.push_back(parameters.bitrate.GetSpatialLayerSum(i) > 0);
    }
  }
  state_observer_->OnRatesUpdated(id_, active_layers);
}

void InstrumentedVideoEncoderWrapper::SetFecControllerOverride(
    webrtc::FecControllerOverride* fec_controller_override) {
  wrapped_encoder_->SetFecControllerOverride(fec_controller_override);
}

void InstrumentedVideoEncoderWrapper::OnPacketLossRateUpdate(
    float packet_loss_rate) {
  wrapped_encoder_->OnPacketLossRateUpdate(packet_loss_rate);
}

void InstrumentedVideoEncoderWrapper::OnRttUpdate(int64_t rtt_ms) {
  wrapped_encoder_->OnRttUpdate(rtt_ms);
}

void InstrumentedVideoEncoderWrapper::OnLossNotification(
    const LossNotification& loss_notification) {
  wrapped_encoder_->OnLossNotification(loss_notification);
}

webrtc::VideoEncoder::EncoderInfo
InstrumentedVideoEncoderWrapper::GetEncoderInfo() const {
  return wrapped_encoder_->GetEncoderInfo();
}

void InstrumentedVideoEncoderWrapper::ReportEncodeResult(
    const VideoEncoderStateObserver::EncodeResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(encoder_sequence_);
  state_observer_->OnEncodedImage(id_, result);
}

webrtc::EncodedImageCallback::Result
InstrumentedVideoEncoderWrapper::OnEncodedImage(
    const webrtc::EncodedImage& encoded_image,
    const webrtc::CodecSpecificInfo* codec_specific_info) {
  VideoEncoderStateObserver::EncodeResult encode_result{
      .width = base::checked_cast<int>(encoded_image._encodedWidth),
      .height = base::checked_cast<int>(encoded_image._encodedHeight),
      .keyframe =
          encoded_image._frameType == webrtc::VideoFrameType::kVideoFrameKey,
      .spatial_index = encoded_image.SpatialIndex(),
      .rtp_timestamp = encoded_image.RtpTimestamp(),
      .encode_end_time = base::TimeTicks::Now(),
      .is_hardware_accelerated = GetEncoderInfo().is_hardware_accelerated};
  if (!encoder_sequence_runner_->RunsTasksInCurrentSequence()) {
    encoder_sequence_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&InstrumentedVideoEncoderWrapper::ReportEncodeResult,
                       weak_this_factory_.GetWeakPtr(), encode_result));
  } else {
    ReportEncodeResult(encode_result);
  }

  webrtc::EncodedImageCallback::Result result(
      webrtc::EncodedImageCallback::Result::OK);
  if (callback_) {
    result = callback_->OnEncodedImage(encoded_image, codec_specific_info);
  }

  return result;
}

void InstrumentedVideoEncoderWrapper::OnDroppedFrame(
    webrtc::EncodedImageCallback::DropReason reason) {
  if (callback_) {
    callback_->OnDroppedFrame(reason);
  }
}
}  // namespace blink
