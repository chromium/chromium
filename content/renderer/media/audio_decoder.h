// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DECODER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DECODER_H_

#include <stddef.h>

#include "content/common/content_export.h"

namespace blink { class WebAudioBus; }

namespace content {

// Decode in-memory audio file data.
CONTENT_EXPORT
bool DecodeAudioFileData(blink::WebAudioBus* destination_bus, const char* data,
                         size_t data_size);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DECODER_H_
