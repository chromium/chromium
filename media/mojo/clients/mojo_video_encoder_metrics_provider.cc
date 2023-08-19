// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "media/base/encoder_status.h"
#include "media/base/svc_scalability_mode.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {
class MojoVideoEncoderMetricsProvider : public VideoEncoderMetricsProvider {
 public:
  MojoVideoEncoderMetricsProvider(
      mojom::VideoEncoderUseCase use_case,
      mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote)
      : use_case_(use_case), pending_remote_(std::move(pending_remote)) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ~MojoVideoEncoderMetricsProvider() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void Initialize(VideoCodecProfile codec_profile,
                  const gfx::Size& encode_size,
                  bool is_hardware_encoder,
                  SVCScalabilityMode svc_mode) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (pending_remote_.is_valid()) {
      remote_.Bind(std::move(pending_remote_));
    }
    initialized_ = true;
    num_encoded_frames_ = 0;
    remote_->Initialize(use_case_, codec_profile, encode_size,
                        is_hardware_encoder, svc_mode);
  }

  void IncrementEncodedFrameCount() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (pending_remote_.is_valid()) {
      DLOG(WARNING) << __func__ << "is called before Initialize()";
      return;
    }
    ++num_encoded_frames_;
    constexpr size_t kEncodedFrameCountBucketSize = 100;
    // Basically update the number of encoded frames every 100 seconds to avoid
    // the frequent mojo call. The exception is the first encoded frames update
    // as it is important to represent whether the encoding actually starts.
    if (num_encoded_frames_ % kEncodedFrameCountBucketSize == 0 ||
        num_encoded_frames_ == 1u) {
      remote_->SetEncodedFrameCount(num_encoded_frames_);
    }
  }

  void SetError(const media::EncoderStatus& status) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (pending_remote_.is_valid()) {
      DLOG(WARNING) << __func__ << "is called before Initialize()";
      return;
    }
    CHECK(!status.is_ok());
    remote_->SetError(status);
  }

 private:
  const mojom::VideoEncoderUseCase use_case_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote_;
  mojo::Remote<mojom::VideoEncoderMetricsProvider> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  size_t num_encoded_frames_ GUARDED_BY_CONTEXT(sequence_checker_){0};
  bool initialized_ GUARDED_BY_CONTEXT(sequence_checker_){false};

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace

std::unique_ptr<VideoEncoderMetricsProvider>
CreateMojoVideoEncoderMetricsProvider(
    mojom::VideoEncoderUseCase use_case,
    mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote) {
  return std::make_unique<MojoVideoEncoderMetricsProvider>(
      use_case, std::move(pending_remote));
}
}  // namespace media
