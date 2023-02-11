// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STABLE_VIDEO_DECODER_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_STABLE_VIDEO_DECODER_FACTORY_H_

#include "content/common/content_export.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

// Binds a StableVideoDecoderFactory PendingReceiver by either using the crosapi
// (on LaCrOS) or starting a new utility process (on non-LaCrOS). This function
// can be called from any thread.
CONTENT_EXPORT void LaunchStableVideoDecoderFactory(
    mojo::PendingReceiver<media::stable::mojom::StableVideoDecoderFactory>
        receiver);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STABLE_VIDEO_DECODER_FACTORY_H_
