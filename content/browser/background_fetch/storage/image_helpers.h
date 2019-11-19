// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_IMAGE_HELPERS_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_IMAGE_HELPERS_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
namespace background_fetch {

using SerializeIconCallback = base::OnceCallback<void(std::string)>;
using DeserializeIconCallback = base::OnceCallback<void(SkBitmap)>;

// Checks whether the icon should be stored on disk. This is only the case for
// non-null |icon|s with a resolution of at most 256x256 pixels.
CONTENT_EXPORT bool ShouldPersistIcon(const SkBitmap& icon);

// Serializes the icon on a separate Task Runner. The |icon| will be serialized
// as a 1x bitmap to raw PNG-encoded data
CONTENT_EXPORT void SerializeIcon(const SkBitmap& icon,
                                  SerializeIconCallback callback);

// Deserializes the icon on a separate Task Runner. he |serialized_icon| must
// contain raw PNG-encoded data, which will be decoded to a 1x bitmap.
CONTENT_EXPORT void DeserializeIcon(
    std::unique_ptr<std::string> serialized_icon,
    DeserializeIconCallback callback);

}  // namespace background_fetch
}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_STORAGE_IMAGE_HELPERS_H_
