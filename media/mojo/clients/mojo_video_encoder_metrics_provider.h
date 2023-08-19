// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_
#define MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_

#include "media/base/video_encoder_metrics_provider.h"
#include "media/mojo/mojom/video_encoder_metrics_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {

std::unique_ptr<VideoEncoderMetricsProvider>
CreateMojoVideoEncoderMetricsProvider(
    mojom::VideoEncoderUseCase use_case,
    mojo::PendingRemote<mojom::VideoEncoderMetricsProvider> pending_remote);
}  // namespace media
#endif  // MEDIA_MOJO_CLIENTS_MOJO_VIDEO_ENCODER_METRICS_PROVIDER_H_
