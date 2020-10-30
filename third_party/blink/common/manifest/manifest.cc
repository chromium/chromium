// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest.h"

namespace blink {

Manifest::ImageResource::ImageResource() = default;

Manifest::ImageResource::ImageResource(const ImageResource& other) = default;

Manifest::ImageResource::~ImageResource() = default;

bool Manifest::ImageResource::operator==(
    const Manifest::ImageResource& other) const {
  return src == other.src && type == other.type && sizes == other.sizes;
}

Manifest::ShortcutItem::ShortcutItem() = default;

Manifest::ShortcutItem::~ShortcutItem() = default;

Manifest::ShareTargetParams::ShareTargetParams() = default;

Manifest::ShareTargetParams::~ShareTargetParams() = default;

Manifest::ShareTarget::ShareTarget() = default;

Manifest::ShareTarget::~ShareTarget() = default;

Manifest::RelatedApplication::RelatedApplication() = default;

Manifest::RelatedApplication::~RelatedApplication() = default;

Manifest::Manifest() = default;

Manifest::Manifest(const Manifest& other) = default;

Manifest::~Manifest() = default;

bool Manifest::IsEmpty() const {
  return !name && !short_name && start_url.is_empty() &&
         display == blink::mojom::DisplayMode::kUndefined &&
         display_override.empty() &&
         orientation == device::mojom::ScreenOrientationLockType::DEFAULT &&
         icons.empty() && shortcuts.empty() && !share_target.has_value() &&
         related_applications.empty() && file_handlers.empty() &&
         !prefer_related_applications && !theme_color && !background_color &&
         !gcm_sender_id && scope.is_empty() && protocol_handlers.empty() &&
         url_handlers.empty();
}

}  // namespace blink
