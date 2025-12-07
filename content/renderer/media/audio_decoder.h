// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DECODER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DECODER_H_

#include <stddef.h>

#include "base/containers/span.h"
#include "content/common/content_export.h"

namespace blink { class WebAudioBus; }

namespace content {

// Decodes encoded audio information passed in `data`. Returned a populated
// audio bus if decoding was successful, otherwise nullptr.
CONTENT_EXPORT
std::unique_ptr<blink::WebAudioBus> DecodeAudioFileData(
    base::span<const char> data);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DECODER_H_
