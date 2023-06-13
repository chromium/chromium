// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "media/base/encoder_status.h"
#include "media/base/svc_scalability_mode.h"
#include "media/mojo/mojom/video_encoder_metrics_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class MojoVideoEncoderMetricsProvider {
 public:
  // MojoVideoEncoderMetricsProvider ctro can be called on any sequence.
  MojoVideoEncoderMetricsProvider(
      mojom::VideoEncoderUseCase use_case,
      mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote);
  virtual ~MojoVideoEncoderMetricsProvider();

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
                  bool is_hardware_encoder) {
    Initialize(codec_profile, encode_size, is_hardware_encoder,
               SVCScalabilityMode::kL1T1);
  }
  virtual void Initialize(VideoCodecProfile codec_profile,
                          const gfx::Size& encode_size,
                          bool is_hardware_encoder,
                          SVCScalabilityMode svc_mode);
  virtual void IncrementEncodedFrameCount();
  virtual void SetError(const media::EncoderStatus& status);

 protected:
  // |sequence_checker_| is used in MockMojoVideoEncoderMetricsProvider.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  const mojom::VideoEncoderUseCase use_case_;
  mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote_;
  mojo::Remote<mojom::VideoEncoderMetricsProvider> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);
  uint64_t num_encoded_frames_ GUARDED_BY_CONTEXT(sequence_checker_){0};
};
}  // namespace media
#endif  // MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_
