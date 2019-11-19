// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_video_encoder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/renderer/video_encode_accelerator.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_descriptor.h"

namespace extensions {

WiFiDisplayVideoEncoder::InitParameters::InitParameters() = default;
WiFiDisplayVideoEncoder::InitParameters::InitParameters(const InitParameters&) =
    default;
WiFiDisplayVideoEncoder::InitParameters::~InitParameters() = default;

WiFiDisplayVideoEncoder::WiFiDisplayVideoEncoder(
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
    : media_task_runner_(std::move(media_task_runner)), send_idr_(false) {
  DCHECK(media_task_runner_);

  // Add descriptors common to all H.264 video encoders.
  descriptors_.push_back(
      WiFiDisplayElementaryStreamDescriptor::AVCTimingAndHRD::Create());
}

WiFiDisplayVideoEncoder::~WiFiDisplayVideoEncoder() = default;

// static
std::vector<wds::H264Profile> WiFiDisplayVideoEncoder::FindSupportedProfiles(
    const gfx::Size& frame_size,
    int32_t frame_rate) {
  std::vector<wds::H264Profile> result;
  media::VideoEncodeAccelerator::SupportedProfiles profiles =
      content::GetSupportedVideoEncodeAcceleratorProfiles();
  for (const auto& supported : profiles) {
    if (supported.profile == media::H264PROFILE_HIGH &&
        supported.max_resolution.width() >= frame_size.width() &&
        supported.max_resolution.height() >= frame_size.height() &&
        supported.max_framerate_numerator >= uint32_t(frame_rate)) {
      result.push_back(wds::CHP);
      break;
    }
  }

  // Constrained profile is provided in any case (by the software encoder
  // implementation).
  result.push_back(wds::CBP);

  return result;
}

// static
void WiFiDisplayVideoEncoder::Create(
    const InitParameters& params,
    const VideoEncoderCallback& encoder_callback) {
  CreateVEA(params, base::Bind(&OnCreatedVEA, params, encoder_callback));
}

// static
void WiFiDisplayVideoEncoder::OnCreatedVEA(
    const InitParameters& params,
    const VideoEncoderCallback& encoder_callback,
    scoped_refptr<WiFiDisplayVideoEncoder> vea_encoder) {
  if (vea_encoder) {
    // An accelerated encoder was created successfully. Pass it on.
    encoder_callback.Run(vea_encoder);
  } else {
    // An accelerated encoder was not created. Fall back to a software encoder.
    CreateSVC(params, encoder_callback);
  }
}

WiFiDisplayElementaryStreamInfo
WiFiDisplayVideoEncoder::CreateElementaryStreamInfo() const {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  return WiFiDisplayElementaryStreamInfo(
      WiFiDisplayElementaryStreamInfo::VIDEO_H264, descriptors_);
}

void WiFiDisplayVideoEncoder::InsertRawVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks reference_time) {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  DCHECK(!encoded_callback_.is_null());
  media_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WiFiDisplayVideoEncoder::InsertFrameOnMediaThread, this,
                     std::move(video_frame), reference_time, send_idr_));
  send_idr_ = false;
}

void WiFiDisplayVideoEncoder::RequestIDRPicture() {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  send_idr_ = true;
}

}  // namespace extensions
