// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/video_encoder_metrics_provider.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace media {

// static
void VideoEncoderMetricsProvider::Create(
    ukm::SourceId source_id,
    mojo::PendingReceiver<mojom::VideoEncoderMetricsProvider> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new VideoEncoderMetricsProvider(source_id)),
      std::move(receiver));
}

VideoEncoderMetricsProvider::VideoEncoderMetricsProvider(
    ukm::SourceId source_id)
    : source_id_(source_id) {}

VideoEncoderMetricsProvider::~VideoEncoderMetricsProvider() {
  ReportUKMIfNeeded();
}

void VideoEncoderMetricsProvider::ReportUKMIfNeeded() const {
  // If Initialize() is not called, no UKM is reported.
  if (!initialized_) {
    return;
  }
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder) {
    return;
  }
  ukm::builders::Media_VideoEncoderMetrics builder(source_id_);
  builder.SetUseCase(static_cast<int>(encoder_use_case_));
  builder.SetProfile(static_cast<int>(codec_profile_));
  builder.SetSVCMode(static_cast<int>(svc_mode_));
  builder.SetIsHardware(is_hardware_encoder_);
  constexpr int kMaxResolutionBucket = 8200;
  builder.SetWidth(
      std::min(encode_size_.width() / 100 * 100, kMaxResolutionBucket));
  builder.SetHeight(
      std::min(encode_size_.height() / 100 * 100, kMaxResolutionBucket));
  builder.SetStatus(static_cast<int>(encoder_status_.code()));

  // We report UKM even if |num_encoded_frames_| is 0 so that we know how
  // Initialize() is called. However, since the number of encoded frame is
  // bucketed per 100, it disables to distinguish Initialize()-only case and
  // the case of encoding a few frames. Therefore, we set the number of encoded
  // frames to 1 if |num_encoded_frames_| is between 1 and 99.
  if (num_encoded_frames_ == 0) {
    builder.SetNumEncodedFrames(0);
  } else {
    builder.SetNumEncodedFrames(
        std::max<uint64_t>(1, num_encoded_frames_ / 100 * 100));
  }
  builder.Record(ukm_recorder);

  // TODO(b/275663480): Report UMAs.
}

void VideoEncoderMetricsProvider::Initialize(
    mojom::VideoEncoderUseCase encoder_use_case,
    VideoCodecProfile codec_profile,
    const gfx::Size& encode_size,
    bool is_hardware_encoder,
    SVCScalabilityMode svc_mode) {
  ReportUKMIfNeeded();
  initialized_ = true;
  num_encoded_frames_ = 0;
  encoder_use_case_ = encoder_use_case;
  codec_profile_ = codec_profile;
  encode_size_ = encode_size;
  is_hardware_encoder_ = is_hardware_encoder;
  encoder_status_ = EncoderStatus::Codes::kOk;
  svc_mode_ = svc_mode;
}

void VideoEncoderMetricsProvider::SetEncodedFrameCount(
    uint64_t num_encoded_frames) {
  num_encoded_frames_ = num_encoded_frames;
}

void VideoEncoderMetricsProvider::SetError(const EncoderStatus& status) {
  CHECK(!status.is_ok());
  if (encoder_status_.is_ok()) {
    encoder_status_ = status;
  }
}

}  // namespace media
