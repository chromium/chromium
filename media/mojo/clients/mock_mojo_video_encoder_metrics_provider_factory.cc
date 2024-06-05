// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mock_mojo_video_encoder_metrics_provider_factory.h"
namespace media {

MockMojoVideoEncoderMetricsProviderFactory::
    MockMojoVideoEncoderMetricsProviderFactory(
        mojom::VideoEncoderUseCase use_case)
    : MojoVideoEncoderMetricsProviderFactory(use_case) {}

MockMojoVideoEncoderMetricsProviderFactory::
    ~MockMojoVideoEncoderMetricsProviderFactory() = default;
}  // namespace media
