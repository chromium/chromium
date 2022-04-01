// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STABLE_VIDEO_DECODER_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_STABLE_VIDEO_DECODER_FACTORY_H_

#include "content/common/content_export.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom-forward.h"

namespace content {

// Returns the browser's remote interface to the global
// StableVideoDecoderFactory which runs out-of-process.
CONTENT_EXPORT media::stable::mojom::StableVideoDecoderFactory&
GetStableVideoDecoderFactory();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STABLE_VIDEO_DECODER_FACTORY_H_
