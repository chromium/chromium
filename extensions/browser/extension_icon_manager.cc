// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_icon_manager.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/icons/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"

namespace extensions {

ExtensionIconManager::ExtensionIconManager() {}

ExtensionIconManager::~ExtensionIconManager() {}

void ExtensionIconManager::LoadIcon(content::BrowserContext* context,
                                    const Extension* extension) {
  // Insert into pending_icons_ first because LoadImage can call us back
  // synchronously if the image is already cached.
  pending_icons_.insert(extension->id());
  ImageLoader* loader = ImageLoader::Get(context);
  loader->LoadImageAtEveryScaleFactorAsync(
      extension, gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize),
      base::BindOnce(&ExtensionIconManager::OnImageLoaded,
                     weak_ptr_factory_.GetWeakPtr(), extension->id()));
}

gfx::Image ExtensionIconManager::GetIcon(const ExtensionId& extension_id) {
  auto iter = icons_.find(extension_id);
  gfx::Image* result = nullptr;
  if (iter == icons_.end()) {
    EnsureDefaultIcon();
    result = &default_icon_;
  } else {
    result = &iter->second;
  }

  DCHECK(result);
  DCHECK_EQ(gfx::kFaviconSize, result->Width());
  DCHECK_EQ(gfx::kFaviconSize, result->Height());
  return *result;
}

void ExtensionIconManager::RemoveIcon(const ExtensionId& extension_id) {
  icons_.erase(extension_id);
  pending_icons_.erase(extension_id);
}

void ExtensionIconManager::OnImageLoaded(const ExtensionId& extension_id,
                                         const gfx::Image& image) {
  if (!image.IsEmpty()) {
    // We may have removed the icon while waiting for it to load. In that case,
    // do nothing.
    if (pending_icons_.erase(extension_id) == 0) {
      return;
    }

    gfx::Image modified_image = image;
    if (monochrome_) {
      color_utils::HSL shift = {-1, 0, 0.6};
      modified_image =
          gfx::Image(gfx::ImageSkiaOperations::CreateHSLShiftedImage(
              image.AsImageSkia(), shift));
    }
    icons_[extension_id] = modified_image;
  }

  if (observer_) {
    observer_->OnImageLoaded(extension_id);
  }
}

void ExtensionIconManager::EnsureDefaultIcon() {
  if (default_icon_.IsEmpty()) {
    default_icon_ = gfx::Image(gfx::CreateVectorIcon(
        vector_icons::kExtensionIcon, gfx::kFaviconSize, gfx::kGoogleGrey700));
  }
}

} // namespace extensions
