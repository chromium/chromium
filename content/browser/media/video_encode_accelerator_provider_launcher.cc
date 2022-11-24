// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/video_encode_accelerator_provider_launcher.h"
#include "content/public/browser/service_process_host.h"

namespace content {

void LaunchVideoEncodeAcceleratorProviderFactory(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProviderFactory>
        receiver) {
  ServiceProcessHost::Launch(
      std::move(receiver),
      ServiceProcessHost::Options().WithDisplayName("Video Encoder").Pass());
}

}  // namespace content