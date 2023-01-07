// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_CHILD_IMAGE_DECODER_UTILS_H_
#define CONTENT_PUBLIC_CHILD_IMAGE_DECODER_UTILS_H_

#include <stddef.h>

#include "content/common/content_export.h"

class SkBitmap;

namespace gfx {
class Size;
}

namespace content {

// Helper function to decode the image using the data passed in.
// On success returns the decoded image.
// On failure returns an empty bitmap.
CONTENT_EXPORT SkBitmap DecodeImage(const unsigned char* data,
                                    const gfx::Size& desired_image_size,
                                    size_t size);
}  // namespace content

#endif  // CONTENT_PUBLIC_CHILD_IMAGE_DECODER_UTILS_H_
