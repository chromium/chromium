// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/cast_software_video_encoder_adapter.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/cast/sender/sender_encoded_frame.h"
#include "media/cast/sender/software_video_encoder.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace remoting {

CastSoftwareVideoEncoderAdapter::CastSoftwareVideoEncoderAdapter(
    std::unique_ptr<media::cast::SoftwareVideoEncoder> encoder,
    webrtc::VideoCodecType codec)
    : encoder_(std::move(encoder)),
      codec_(codec) {
  DCHECK(encoder_);
}

CastSoftwareVideoEncoderAdapter::~CastSoftwareVideoEncoderAdapter() = default;

void CastSoftwareVideoEncoderAdapter::Encode(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    const FrameParams& params,
    EncodeCallback done) {
  const base::TimeTicks current_time = base::TimeTicks::Now();
  if (start_timestamp_.is_null()) {
    encoder_->Initialize();
    start_timestamp_ = current_time;
  }

  if (params.key_frame) {
    encoder_->GenerateKeyFrame();
  }
  if (params.bitrate_kbps > 0) {
    encoder_->UpdateRates(params.bitrate_kbps * 1000);
  }

  media::cast::SenderEncodedFrame encoded_frame;
  encoder_->Encode(CreateVideoFrame(*frame),
                   current_time,
                   &encoded_frame);
  std::move(done).Run(EncodeResult::SUCCEEDED,
                      CreateEncodedFrame(*frame, std::move(encoded_frame)));
}

scoped_refptr<media::VideoFrame>
CastSoftwareVideoEncoderAdapter::CreateVideoFrame(
    const webrtc::DesktopFrame& frame) const {
  DCHECK(!start_timestamp_.is_null());
  // TODO(zijiehe): According to http://crbug.com/555909, this does not work
  // now, media::VideoFrame::WrapExternalData() accepts only I420 or Y16.
  return media::VideoFrame::WrapExternalData(
      media::PIXEL_FORMAT_ARGB,
      gfx::Size(frame.stride() / webrtc::DesktopFrame::kBytesPerPixel,
                frame.size().height()),
      gfx::Rect(frame.size().width(), frame.size().height()),
      gfx::Size(frame.size().width(), frame.size().height()),
      frame.data(),
      frame.stride() * frame.size().height(),
      base::TimeTicks::Now() - start_timestamp_);
}

std::unique_ptr<CastSoftwareVideoEncoderAdapter::EncodedFrame>
CastSoftwareVideoEncoderAdapter::CreateEncodedFrame(
    const webrtc::DesktopFrame& frame,
    media::cast::SenderEncodedFrame&& media_frame) const {
  std::unique_ptr<EncodedFrame> result = std::make_unique<EncodedFrame>();
  result->size = frame.size();
  // TODO(zijiehe): Should INDEPENDENT frames also be considered as key frames?
  result->key_frame =
      media_frame.dependency == media::cast::EncodedFrame::KEY;
  // TODO(zijiehe): Forward quantizer from media::cast::SoftwareVideoEncoder.
  // Currently we set this value to INT32_MAX to always trigger top-off.
  result->quantizer = INT32_MAX;
  result->codec = codec_;
  result->data = std::move(media_frame.data);
  return result;
}

}  // namespace remoting
