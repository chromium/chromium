// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mock_mojo_video_encoder_metrics_provider.h"

namespace media {
MockMojoVideoEncoderMetricsProvider::MockMojoVideoEncoderMetricsProvider(
    mojom::VideoEncoderUseCase use_case)
    : MojoVideoEncoderMetricsProvider(
          use_case,
          mojo::PendingRemote<mojom::VideoEncoderMetricsProvider>()) {}

MockMojoVideoEncoderMetricsProvider::~MockMojoVideoEncoderMetricsProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MockDestroy();
}
}  // namespace media
