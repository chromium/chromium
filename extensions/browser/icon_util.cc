// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/icon_util.h"

#include "base/base64.h"
#include "base/memory/ref_counted_memory.h"
#include "extensions/browser/extension_icon_placeholder.h"
#include "extensions/common/constants.h"
#include "skia/public/mojom/bitmap.mojom.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "url/gurl.h"

namespace extensions {

// The icon size parsed should always be the same as the icon size of the
// extension action.
// LINT.IfChange(ActionIconSize)
#if BUILDFLAG(IS_ANDROID)
constexpr int kActionIconSize = extension_misc::EXTENSION_ICON_SMALLISH;
#else
constexpr int kActionIconSize = extension_misc::EXTENSION_ICON_BITTY;
#endif
// LINT.ThenChange(/extensions/browser/extension_action.cc:ActionIconSize)

IconParseResult ParseIconFromCanvasDictionary(const base::Value::Dict& dict,
                                              gfx::ImageSkia* icon) {
  for (const auto item : dict) {
    std::string byte_string;
    const void* bytes = nullptr;
    size_t num_bytes = 0;
    if (item.second.is_blob()) {
      bytes = item.second.GetBlob().data();
      num_bytes = item.second.GetBlob().size();
    } else if (item.second.is_string()) {
      if (!base::Base64Decode(item.second.GetString(), &byte_string)) {
        return IconParseResult::kDecodeFailure;
      }
      bytes = byte_string.c_str();
      num_bytes = byte_string.length();
    } else {
      continue;
    }
    SkBitmap bitmap;
    if (!skia::mojom::InlineBitmap::Deserialize(bytes, num_bytes, &bitmap)) {
      return IconParseResult::kUnpickleFailure;
    }
    // A well-behaved renderer will never send a null bitmap to us here.
    CHECK(!bitmap.isNull());

    // Chrome helpfully scales the provided icon(s), but let's not go overboard.
    const int kActionIconMaxSize = 10 * kActionIconSize;
    if (bitmap.drawsNothing() || bitmap.width() > kActionIconMaxSize) {
      continue;
    }

    float scale = static_cast<float>(bitmap.width()) / kActionIconSize;
    icon->AddRepresentation(gfx::ImageSkiaRep(bitmap, scale));
  }
  return IconParseResult::kSuccess;
}

GURL GetPlaceholderIconUrl(extension_misc::ExtensionIcons icon_size,
                           const std::string& name) {
  return GetIconUrlFromImage(
      ExtensionIconPlaceholder::CreateImage(icon_size, name));
}

GURL GetIconUrlFromImage(const gfx::Image& image) {
  std::string base_64 = base::Base64Encode(*image.As1xPNGBytes());
  const char kDataUrlPrefix[] = "data:image/png;base64,";
  return GURL(kDataUrlPrefix + base_64);
}

}  // namespace extensions
