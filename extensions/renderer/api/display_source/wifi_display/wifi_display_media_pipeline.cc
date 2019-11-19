// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_pipeline.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/video_encode_accelerator.h"

namespace extensions {

namespace {

const char kErrorAudioEncoderError[] = "Unrepairable audio encoder error";
const char kErrorVideoEncoderError[] = "Unrepairable video encoder error";
const char kErrorUnableSendMedia[] = "Unable to send media";

}  // namespace

WiFiDisplayMediaPipeline::WiFiDisplayMediaPipeline(
    wds::SessionType type,
    const WiFiDisplayVideoEncoder::InitParameters& video_parameters,
    const wds::AudioCodec& audio_codec,
    const net::IPAddress& sink_ip_address,
    const std::pair<int, int>& sink_rtp_ports,
    const RegisterMediaServiceCallback& service_callback,
    const ErrorCallback& error_callback)
    : type_(type),
      video_parameters_(video_parameters),
      audio_codec_(audio_codec),
      sink_ip_address_(sink_ip_address),
      sink_rtp_ports_(sink_rtp_ports),
      service_callback_(service_callback),
      error_callback_(error_callback),
      weak_factory_(this) {}

// static
std::unique_ptr<WiFiDisplayMediaPipeline> WiFiDisplayMediaPipeline::Create(
    wds::SessionType type,
    const WiFiDisplayVideoEncoder::InitParameters& video_parameters,
    const wds::AudioCodec& audio_codec,
    const net::IPAddress& sink_ip_address,
    const std::pair<int, int>& sink_rtp_ports,
    const RegisterMediaServiceCallback& service_callback,
    const ErrorCallback& error_callback) {
  return std::unique_ptr<WiFiDisplayMediaPipeline>(
      new WiFiDisplayMediaPipeline(type,
                                   video_parameters,
                                   audio_codec,
                                   sink_ip_address,
                                   sink_rtp_ports,
                                   service_callback,
                                   error_callback));
}

WiFiDisplayMediaPipeline::~WiFiDisplayMediaPipeline() {
}

void WiFiDisplayMediaPipeline::InsertRawVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks reference_time) {
  DCHECK(video_encoder_);
  video_encoder_->InsertRawVideoFrame(std::move(video_frame), reference_time);
}

void WiFiDisplayMediaPipeline::RequestIDRPicture() {
  DCHECK(video_encoder_);
  video_encoder_->RequestIDRPicture();
}

enum class WiFiDisplayMediaPipeline::InitializationStep : unsigned {
  FIRST,
  AUDIO_ENCODER = FIRST,
  VIDEO_ENCODER,
  MEDIA_PACKETIZER,
  MEDIA_SERVICE,
  LAST = MEDIA_SERVICE
};

void WiFiDisplayMediaPipeline::Initialize(
    const InitCompletionCallback& callback) {
  DCHECK(!audio_encoder_ && !video_encoder_ && !packetizer_);
  OnInitialize(callback, InitializationStep::FIRST, true);
}

void WiFiDisplayMediaPipeline::OnInitialize(
    const InitCompletionCallback& callback,
    InitializationStep current_step,
    bool success) {
  if (!success) {
    callback.Run(false);
    return;
  }

  InitStepCompletionCallback init_step_callback;
  if (current_step < InitializationStep::LAST) {
    InitializationStep next_step = static_cast<InitializationStep>(
        static_cast<unsigned>(current_step) + 1u);
    init_step_callback =
        base::Bind(&WiFiDisplayMediaPipeline::OnInitialize,
                   weak_factory_.GetWeakPtr(), callback, next_step);
  }

  switch (current_step) {
    case InitializationStep::AUDIO_ENCODER:
      DCHECK(!audio_encoder_);
      if (type_ & wds::AudioSession) {
        auto result_callback =
            base::Bind(&WiFiDisplayMediaPipeline::OnAudioEncoderCreated,
                       weak_factory_.GetWeakPtr(), init_step_callback);
        WiFiDisplayAudioEncoder::Create(audio_codec_, result_callback);
      } else {
        init_step_callback.Run(true);
      }
      break;
    case InitializationStep::VIDEO_ENCODER:
      DCHECK(!video_encoder_);
      if (type_ & wds::VideoSession) {
        auto result_callback =
            base::Bind(&WiFiDisplayMediaPipeline::OnVideoEncoderCreated,
                       weak_factory_.GetWeakPtr(), init_step_callback);
        WiFiDisplayVideoEncoder::Create(video_parameters_, result_callback);
      } else {
        init_step_callback.Run(true);
      }
      break;
    case InitializationStep::MEDIA_PACKETIZER:
      DCHECK(!packetizer_);
      CreateMediaPacketizer();
      init_step_callback.Run(true);
      break;
    case InitializationStep::MEDIA_SERVICE:
      service_callback_.Run(
          media_service_.BindNewPipeAndPassReceiver(),
          base::Bind(&WiFiDisplayMediaPipeline::OnMediaServiceRegistered,
                     weak_factory_.GetWeakPtr(), callback));
      break;
  }
}

void WiFiDisplayMediaPipeline::CreateMediaPacketizer() {
  DCHECK(!packetizer_);
  std::vector<WiFiDisplayElementaryStreamInfo> stream_infos;

  if (type_ & wds::VideoSession) {
    DCHECK(video_encoder_);
    stream_infos.push_back(video_encoder_->CreateElementaryStreamInfo());
  }

  if (type_ & wds::AudioSession) {
    DCHECK(audio_encoder_);
    stream_infos.push_back(audio_encoder_->CreateElementaryStreamInfo());
  }

  packetizer_.reset(new WiFiDisplayMediaPacketizer(
      base::TimeDelta::FromMilliseconds(200), stream_infos,
      base::Bind(&WiFiDisplayMediaPipeline::OnPacketizedMediaDatagramPacket,
                 base::Unretained(this))));
}

void WiFiDisplayMediaPipeline::OnAudioEncoderCreated(
    const InitStepCompletionCallback& callback,
    scoped_refptr<WiFiDisplayAudioEncoder> audio_encoder) {
  DCHECK(!audio_encoder_);

  if (!audio_encoder) {
    callback.Run(false);
    return;
  }

  audio_encoder_ = std::move(audio_encoder);
  auto encoded_callback =
      base::Bind(&WiFiDisplayMediaPipeline::OnEncodedAudioUnit,
                 weak_factory_.GetWeakPtr());
  auto error_callback = base::Bind(error_callback_, kErrorAudioEncoderError);
  audio_encoder_->SetCallbacks(encoded_callback, error_callback);

  callback.Run(true);
}

void WiFiDisplayMediaPipeline::OnVideoEncoderCreated(
    const InitStepCompletionCallback& callback,
    scoped_refptr<WiFiDisplayVideoEncoder> video_encoder) {
  DCHECK(!video_encoder_);

  if (!video_encoder) {
    callback.Run(false);
    return;
  }

  video_encoder_ = std::move(video_encoder);
  auto encoded_callback = base::Bind(
      &WiFiDisplayMediaPipeline::OnEncodedVideoFrame,
      weak_factory_.GetWeakPtr());
  auto error_callback = base::Bind(error_callback_, kErrorVideoEncoderError);
  video_encoder_->SetCallbacks(encoded_callback, error_callback);

  callback.Run(true);
}

void WiFiDisplayMediaPipeline::OnMediaServiceRegistered(
    const InitCompletionCallback& callback) {
  DCHECK(media_service_);
  auto error_callback = base::Bind(error_callback_, kErrorUnableSendMedia);
  media_service_.set_disconnect_handler(error_callback);
  media_service_->SetDestinationPoint(
      net::IPEndPoint(sink_ip_address_,
                      static_cast<uint16_t>(sink_rtp_ports_.first)),
      callback);
}

void WiFiDisplayMediaPipeline::OnEncodedAudioUnit(
    std::unique_ptr<WiFiDisplayEncodedFrame> unit) {
  DCHECK(packetizer_);
  const unsigned stream_index = (type_ & wds::VideoSession) ? 1u : 0u;
  if (!packetizer_->EncodeElementaryStreamUnit(stream_index, unit->bytes(),
                                               unit->size(), unit->key_frame,
                                               unit->pts, unit->dts, true)) {
    DVLOG(1) << "Couldn't write audio mpegts packet";
  }
}

void WiFiDisplayMediaPipeline::OnEncodedVideoFrame(
    std::unique_ptr<WiFiDisplayEncodedFrame> frame) {
  DCHECK(packetizer_);
  if (!packetizer_->EncodeElementaryStreamUnit(0u, frame->bytes(),
                                               frame->size(), frame->key_frame,
                                               frame->pts, frame->dts, true)) {
    DVLOG(1) << "Couldn't write video mpegts packet";
  }
}

bool WiFiDisplayMediaPipeline::OnPacketizedMediaDatagramPacket(
    WiFiDisplayMediaDatagramPacket media_datagram_packet) {
  DCHECK(media_service_);
  mojom::WiFiDisplayMediaPacketPtr packet =
      mojom::WiFiDisplayMediaPacket::New();
  packet->data = std::move(media_datagram_packet);
  media_service_->SendMediaPacket(std::move(packet));
  return true;
}

}  // namespace extensions
