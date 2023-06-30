// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOCK_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_
#define MEDIA_MOJO_CLIENTS_MOCK_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_

#include "base/sequence_checker.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
class MockMojoVideoEncoderMetricsProvider
    : public MojoVideoEncoderMetricsProvider {
 public:
  explicit MockMojoVideoEncoderMetricsProvider(
      mojom::VideoEncoderUseCase use_case);
  ~MockMojoVideoEncoderMetricsProvider() override;

  void Initialize(VideoCodecProfile codec_profile,
                  const gfx::Size& encode_size,
                  bool is_hardware_encoder,
                  SVCScalabilityMode svc_mode) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    MockInitialize(codec_profile, encode_size, is_hardware_encoder, svc_mode);
  }
  void IncrementEncodedFrameCount() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    MockIncrementEncodedFrameCount();
  }
  void SetError(const media::EncoderStatus& status) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    MockSetError(status);
  }

  MOCK_METHOD(
      void,
      MockInitialize,
      (VideoCodecProfile, (const gfx::Size&), bool, SVCScalabilityMode));
  MOCK_METHOD(void, MockIncrementEncodedFrameCount, ());
  MOCK_METHOD(void, MockSetError, (const media::EncoderStatus&));
  MOCK_METHOD(void, MockDestroy, ());
};
}  // namespace media
#endif  // MEDIA_MOJO_CLIENTS_MOCK_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_
