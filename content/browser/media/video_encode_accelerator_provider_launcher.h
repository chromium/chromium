// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_VIDEO_ENCODE_ACCELERATOR_PROVIDER_LAUNCHER_H_
#define CONTENT_BROWSER_MEDIA_VIDEO_ENCODE_ACCELERATOR_PROVIDER_LAUNCHER_H_

#include "media/mojo/mojom/video_encode_accelerator.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

// This method is used to create a utility process that hosts a
// VideoEncodeAcceleratorProviderFactory implementation so that video
// encode acceleration can be done outside of the GPU process.
void LaunchVideoEncodeAcceleratorProviderFactory(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProviderFactory>
        receiver);

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_VIDEO_ENCODE_ACCELERATOR_PROVIDER_LAUNCHER_H_