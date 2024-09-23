// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_SERVICE_H_

#include "media/mojo/mojom/video_encoder_metrics_provider.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

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
  void Initialize(uint64_t encoder_id,
                  mojom::VideoEncoderUseCase encoder_use_case,
                  VideoCodecProfile codec_profile,
                  const gfx::Size& encode_size,
                  bool is_hardware_encoder,
                  SVCScalabilityMode svc_mode) override;
  void SetEncodedFrameCount(uint64_t encoder_id,
                            uint64_t num_encoded_frames) override;
  void SetError(uint64_t encoder_id, const EncoderStatus& status) override;
  void Complete(uint64_t encoder_id) override;

 private:
  class EncoderMetricsHandler;

  explicit MojoVideoEncoderMetricsProviderService(ukm::SourceId source_id);

  const ukm::SourceId source_id_;

  std::map<uint64_t, EncoderMetricsHandler> encoders_;
};
}  // namespace media
#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_SERVICE_H_
