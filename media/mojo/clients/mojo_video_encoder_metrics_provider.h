// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/mojo/mojom/video_encoder_metrics_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
class MojoVideoEncoderMetricsProvider : public VideoEncoderMetricsProvider {
 public:
  // MojoVideoEncoderMetricsProvider ctro can be called on any sequence.
  MojoVideoEncoderMetricsProvider(
      mojom::VideoEncoderUseCase use_case,
      mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote);
  ~MojoVideoEncoderMetricsProvider() override;

  MojoVideoEncoderMetricsProvider(const MojoVideoEncoderMetricsProvider&) =
      delete;
  MojoVideoEncoderMetricsProvider& operator=(
      const MojoVideoEncoderMetricsProvider&) = delete;
  MojoVideoEncoderMetricsProvider(MojoVideoEncoderMetricsProvider&&) = delete;
  MojoVideoEncoderMetricsProvider& operator=(
      MojoVideoEncoderMetricsProvider&&) = delete;

  // All of the function must be called on the same sequence.
  void Initialize(VideoCodecProfile codec_profile,
                  const gfx::Size& encode_size,
                  bool is_hardware_encoder,
                  SVCScalabilityMode svc_mode) override;
  void IncrementEncodedFrameCount() override;
  void SetError(const media::EncoderStatus& status) override;

 private:
  const mojom::VideoEncoderUseCase use_case_;
  mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote_;
  mojo::Remote<mojom::VideoEncoderMetricsProvider> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  uint64_t num_encoded_frames_ GUARDED_BY_CONTEXT(sequence_checker_){0};

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace media
#endif  // MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_
