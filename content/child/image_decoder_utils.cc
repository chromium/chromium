// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/child/image_decoder_utils.h"

#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/web/web_image.h"
#include "third_party/skia/include/core/SkBitmap.h"

using blink::WebData;
using blink::WebImage;

namespace content {

SkBitmap DecodeImage(base::span<const unsigned char> data,
                     const gfx::Size& desired_image_size) {
  return WebImage::FromData(WebData(data), desired_image_size);
}

}  // namespace content
