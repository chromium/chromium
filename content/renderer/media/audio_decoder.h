// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_AUDIO_DECODER_H_
#define CONTENT_RENDERER_MEDIA_AUDIO_DECODER_H_

#include <stddef.h>

#include <memory>

#include "base/containers/span.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/platform/platform.h"

namespace content {

// Decodes encoded audio information passed in `data`. Returns a populated
// decoded audio file if decoding was successful, otherwise nullptr.
CONTENT_EXPORT
std::unique_ptr<blink::Platform::DecodedAudioFile> DecodeAudioFileData(
    base::span<const char> data);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_AUDIO_DECODER_H_
