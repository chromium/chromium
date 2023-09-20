// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_encoder_metrics_provider_service.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace media {

class MojoVideoEncoderMetricsProviderService::EncoderMetricsHandler {
 public:
  EncoderMetricsHandler(const ukm::SourceId source_id,
                        mojom::VideoEncoderUseCase encoder_use_case,
                        VideoCodecProfile codec_profile,
                        const gfx::Size& encode_size,
                        bool is_hardware_encoder,
                        SVCScalabilityMode svc_mode)
      : source_id_(source_id),
        encoder_use_case_(encoder_use_case),
        codec_profile_(codec_profile),
        encode_size_(encode_size),
        is_hardware_encoder_(is_hardware_encoder),
        svc_mode_(svc_mode) {}
  ~EncoderMetricsHandler() { ReportMetrics(); }

  EncoderMetricsHandler(EncoderMetricsHandler&& handler)
      : source_id_(handler.source_id_),
        encoder_use_case_(handler.encoder_use_case_),
        codec_profile_(handler.codec_profile_),
        encode_size_(handler.encode_size_),
        is_hardware_encoder_(handler.is_hardware_encoder_),
        svc_mode_(handler.svc_mode_),
        report_metrics_(true),
        encoder_status_(handler.encoder_status_),
        num_encoded_frames_(handler.num_encoded_frames_) {
    // Set report ukm is false because the ukm should be reported in the created
    // EncoderMetricsHandler.
    handler.report_metrics_ = false;
  }

  EncoderMetricsHandler(const EncoderMetricsHandler&) = delete;
  EncoderMetricsHandler& operator=(const EncoderMetricsHandler&) = delete;

  void SetEncodedFrameCount(uint64_t num_encoded_frames) {
    num_encoded_frames_ = num_encoded_frames;
  }

  void SetError(const media::EncoderStatus& status) {
    CHECK(!status.is_ok());
    if (encoder_status_.is_ok()) {
      encoder_status_ = status;
    }
  }

 private:
  void ReportMetrics() const {
    if (!report_metrics_) {
      return;
    }

    ReportUKM();
    ReportUMA();
  }

  void ReportUKM() const {
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
    // the case of encoding a few frames. Therefore, we set the number of
    // encoded frames to 1 if |num_encoded_frames_| is between 1 and 99.
    if (num_encoded_frames_ == 0) {
      builder.SetNumEncodedFrames(0);
    } else {
      builder.SetNumEncodedFrames(
          std::max<uint64_t>(1, num_encoded_frames_ / 100 * 100));
    }
    builder.Record(ukm_recorder);
  }

  void ReportUMA() const {
    std::string uma_prefix = "Media.VideoEncoder.";
    switch (encoder_use_case_) {
      case mojom::VideoEncoderUseCase::kCastMirroring:
        uma_prefix += "CastMirroring.";
        break;
      case mojom::VideoEncoderUseCase::kMediaRecorder:
        uma_prefix += "MediaRecorder.";
        break;
      case mojom::VideoEncoderUseCase::kWebCodecs:
        uma_prefix += "WebCodecs.";
        break;
      case mojom::VideoEncoderUseCase::kWebRTC:
        uma_prefix += "WebRTC.";
        break;
    }

    base::UmaHistogramEnumeration(
        base::StrCat({uma_prefix, "Profile"}), codec_profile_,
        static_cast<VideoCodecProfile>(VIDEO_CODEC_PROFILE_MAX + 1));
    base::UmaHistogramEnumeration(base::StrCat({uma_prefix, "SVC"}), svc_mode_);
    base::UmaHistogramBoolean(base::StrCat({uma_prefix, "HW"}),
                              is_hardware_encoder_);
    base::UmaHistogramCounts10000(base::StrCat({uma_prefix, "Width"}),
                                  encode_size_.width());
    base::UmaHistogramCounts10000(base::StrCat({uma_prefix, "Height"}),
                                  encode_size_.height());
    base::UmaHistogramCounts1M(base::StrCat({uma_prefix, "Area"}),
                               encode_size_.GetArea() / 100);
    base::UmaHistogramEnumeration(base::StrCat({uma_prefix, "Status"}),
                                  encoder_status_.code());
    // One million frames is about 9.25 hours in 30 fps. That should be enough
    // thinking of the encoder is recreated often.
    base::UmaHistogramCounts1M(base::StrCat({uma_prefix, "Frames"}),
                               num_encoded_frames_);
  }

  const ukm::SourceId source_id_;
  const mojom::VideoEncoderUseCase encoder_use_case_;
  const VideoCodecProfile codec_profile_;
  const gfx::Size encode_size_;
  const bool is_hardware_encoder_;
  const SVCScalabilityMode svc_mode_;

  bool report_metrics_ = true;

  EncoderStatus encoder_status_ = EncoderStatus::Codes::kOk;
  uint64_t num_encoded_frames_ = 0;
};

// static
void MojoVideoEncoderMetricsProviderService::Create(
    ukm::SourceId source_id,
    mojo::PendingReceiver<mojom::VideoEncoderMetricsProvider> receiver) {
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new MojoVideoEncoderMetricsProviderService(source_id)),
      std::move(receiver));
}

MojoVideoEncoderMetricsProviderService::MojoVideoEncoderMetricsProviderService(
    ukm::SourceId source_id)
    : source_id_(source_id) {}

MojoVideoEncoderMetricsProviderService::
    ~MojoVideoEncoderMetricsProviderService() = default;

void MojoVideoEncoderMetricsProviderService::Initialize(
    uint64_t encoder_id,
    mojom::VideoEncoderUseCase encoder_use_case,
    VideoCodecProfile codec_profile,
    const gfx::Size& encode_size,
    bool is_hardware_encoder,
    SVCScalabilityMode svc_mode) {
  encoders_.erase(encoder_id);
  encoders_.emplace(
      encoder_id,
      EncoderMetricsHandler(source_id_, encoder_use_case, codec_profile,
                            encode_size, is_hardware_encoder, svc_mode));
}

void MojoVideoEncoderMetricsProviderService::SetEncodedFrameCount(
    uint64_t encoder_id,
    uint64_t num_encoded_frames) {
  auto it = encoders_.find(encoder_id);
  if (it == encoders_.end()) {
    mojo::ReportBadMessage(base::StrCat(
        {"Unknown encoder id: ", base::NumberToString(encoder_id)}));
    return;
  }
  it->second.SetEncodedFrameCount(num_encoded_frames);
}

void MojoVideoEncoderMetricsProviderService::SetError(
    uint64_t encoder_id,
    const EncoderStatus& status) {
  auto it = encoders_.find(encoder_id);
  if (it == encoders_.end()) {
    mojo::ReportBadMessage(base::StrCat(
        {"Unknown encoder id: ", base::NumberToString(encoder_id)}));
    return;
  }
  it->second.SetError(status);
}

void MojoVideoEncoderMetricsProviderService::Complete(uint64_t encoder_id) {
  if (encoders_.erase(encoder_id) == 0u) {
    mojo::ReportBadMessage(base::StrCat(
        {"Unknown encoder id: ", base::NumberToString(encoder_id)}));
  }
}
}  // namespace media
