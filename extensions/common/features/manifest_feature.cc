// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/manifest_feature.h"

#include "base/values.h"
#include "extensions/common/manifest.h"
#include "extensions/common/mojom/context_type.mojom.h"

namespace extensions {

ManifestFeature::ManifestFeature() {
}

ManifestFeature::~ManifestFeature() {
}

Feature::Availability ManifestFeature::IsAvailableToContextImpl(
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    Feature::Platform platform,
    int context_id,
    bool check_developer_mode,
    const ContextData& context_data) const {
  Availability availability = SimpleFeature::IsAvailableToContextImpl(
      extension, context, url, platform, context_id, check_developer_mode,
      context_data);
  if (!availability.is_available())
    return availability;

  // We know we can skip manifest()->GetKey() here because we just did the same
  // validation it would do above.
  if (extension && !extension->manifest()->value()->contains(name()))
    return CreateAvailability(NOT_PRESENT, extension->GetType());

  return CreateAvailability(IS_AVAILABLE);
}

}  // namespace extensions
