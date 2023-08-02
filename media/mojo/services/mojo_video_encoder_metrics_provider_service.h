// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_SERVICE_H_

#include "media/base/encoder_status.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_codecs.h"
#include "media/mojo/mojom/video_encoder_metrics_provider.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// See mojom::VideoEncoderMetricsProvider for documentation.
class MEDIA_MOJO_EXPORT MojoVideoEncoderMetricsProviderService
    : public mojom::VideoEncoderMetricsProvider {
 public:
  static void Create(
      ukm::SourceId source_id,
      mojo::PendingReceiver<mojom::VideoEncoderMetricsProvider> receiver);

  ~MojoVideoEncoderMetricsProviderService() override;

  // mojom::VideoEncoderMetricsProvider implementation.
  void Initialize(mojom::VideoEncoderUseCase encoder_use_case,
                  VideoCodecProfile codec_profile,
                  const gfx::Size& encode_size,
                  bool is_hardware_encoder,
                  SVCScalabilityMode svc_mode) override;
  void SetEncodedFrameCount(uint64_t num_encoded_frames) override;
  void SetError(const EncoderStatus& status) override;

 private:
  explicit MojoVideoEncoderMetricsProviderService(ukm::SourceId source_id);

  void ReportUKMIfNeeded() const;

  const ukm::SourceId source_id_;
  bool initialized_ = false;
  uint64_t num_encoded_frames_ = 0;

  mojom::VideoEncoderUseCase encoder_use_case_;
  VideoCodecProfile codec_profile_;
  gfx::Size encode_size_;
  bool is_hardware_encoder_;
  SVCScalabilityMode svc_mode_;
  EncoderStatus encoder_status_;
};
}  // namespace media
#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_SERVICE_H_
