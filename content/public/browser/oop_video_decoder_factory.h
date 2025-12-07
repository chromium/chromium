// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_OOP_VIDEO_DECODER_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_OOP_VIDEO_DECODER_FACTORY_H_

#include "content/common/content_export.h"
#include "media/mojo/mojom/interface_factory.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/viz/public/mojom/gpu.mojom-forward.h"

namespace content {

// Binds a media::mojom::InterfaceFactory PendingReceiver by starting a new
// utility process. This function can be called from any thread.
CONTENT_EXPORT void LaunchOOPVideoDecoderFactory(
    mojo::PendingReceiver<media::mojom::InterfaceFactory> receiver,
    mojo::PendingRemote<viz::mojom::Gpu> gpu_remote);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_OOP_VIDEO_DECODER_FACTORY_H_
