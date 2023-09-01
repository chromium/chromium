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
class MojoVideoEncoderMetricsProviderFactory::MojoVideoEncoderMetricsProvider
    : public VideoEncoderMetricsProvider {
 public:
  MojoVideoEncoderMetricsProvider(
      scoped_refptr<MojoVideoEncoderMetricsProviderFactory> factory,
      mojom::VideoEncoderUseCase use_case,
      uint64_t encoder_id)
      : factory_(std::move(factory)),
        use_case_(use_case),
        encoder_id_(encoder_id) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ~MojoVideoEncoderMetricsProvider() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (initialized_) {
      CHECK(remote_);
      (*remote_)->Complete(encoder_id_);
    }
  }

  void Initialize(VideoCodecProfile codec_profile,
                  const gfx::Size& encode_size,
                  bool is_hardware_encoder,
                  SVCScalabilityMode svc_mode) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    initialized_ = true;
    num_encoded_frames_ = 0;
    if (!remote_) {
      remote_ = factory_->GetRemote();
    }
    (*remote_)->Initialize(encoder_id_, use_case_, codec_profile, encode_size,
                           is_hardware_encoder, svc_mode);
  }

  void IncrementEncodedFrameCount() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!remote_) {
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
      (*remote_)->SetEncodedFrameCount(encoder_id_, num_encoded_frames_);
    }
  }

  void SetError(const media::EncoderStatus& status) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!status.is_ok());
    if (!remote_) {
      DLOG(WARNING) << __func__ << "is called before Initialize()";
      return;
    }
    (*remote_)->SetError(encoder_id_, status);
  }

 private:
  // To guarantee |remote_| is valid as long as MojoVideoEncoderMetricsProvider
  // is alive.
  const scoped_refptr<MojoVideoEncoderMetricsProviderFactory> factory_;

  raw_ptr<mojo::Remote<mojom::VideoEncoderMetricsProvider>> remote_
      GUARDED_BY_CONTEXT(sequence_checker_){nullptr};

  const mojom::VideoEncoderUseCase use_case_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const uint64_t encoder_id_ GUARDED_BY_CONTEXT(sequence_checker_);

  size_t num_encoded_frames_ GUARDED_BY_CONTEXT(sequence_checker_){0};
  bool initialized_ GUARDED_BY_CONTEXT(sequence_checker_){false};

  SEQUENCE_CHECKER(sequence_checker_);
};

MojoVideoEncoderMetricsProviderFactory::MojoVideoEncoderMetricsProviderFactory(
    mojom::VideoEncoderUseCase use_case,
    mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote)
    : use_case_(use_case), pending_remote_(std::move(pending_remote)) {
  DETACH_FROM_SEQUENCE(remote_sequence_checker_);
  DETACH_FROM_SEQUENCE(create_provider_sequence_checker_);
}

MojoVideoEncoderMetricsProviderFactory::
    ~MojoVideoEncoderMetricsProviderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(remote_sequence_checker_);
}

MojoVideoEncoderMetricsProviderFactory::MojoVideoEncoderMetricsProviderFactory(
    mojom::VideoEncoderUseCase use_case)
    : use_case_(use_case) {
  DETACH_FROM_SEQUENCE(remote_sequence_checker_);
  DETACH_FROM_SEQUENCE(create_provider_sequence_checker_);
}

mojo::Remote<mojom::VideoEncoderMetricsProvider>*
MojoVideoEncoderMetricsProviderFactory::GetRemote() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(remote_sequence_checker_);
  if (pending_remote_.is_valid()) {
    remote_.Bind(std::move(pending_remote_));
  }
  return &remote_;
}

std::unique_ptr<VideoEncoderMetricsProvider>
MojoVideoEncoderMetricsProviderFactory::CreateVideoEncoderMetricsProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(create_provider_sequence_checker_);
  return std::make_unique<MojoVideoEncoderMetricsProvider>(this, use_case_,
                                                           encoder_id_++);
}
}  // namespace media
