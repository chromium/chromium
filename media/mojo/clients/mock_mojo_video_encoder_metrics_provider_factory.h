// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOCK_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_FACTORY_H_
#define MEDIA_MOJO_CLIENTS_MOCK_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_FACTORY_H_

#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
class MockMojoVideoEncoderMetricsProviderFactory
    : public media::MojoVideoEncoderMetricsProviderFactory {
 public:
  explicit MockMojoVideoEncoderMetricsProviderFactory(
      mojom::VideoEncoderUseCase use_case);

  MOCK_METHOD(std::unique_ptr<media::VideoEncoderMetricsProvider>,
              CreateVideoEncoderMetricsProvider,
              (),
              (override));

 protected:
  ~MockMojoVideoEncoderMetricsProviderFactory() override;
};
}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOCK_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_FACTORY_H_
