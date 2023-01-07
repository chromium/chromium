// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_zoom_request_client.h"

#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

namespace extensions {

ExtensionZoomRequestClient::ExtensionZoomRequestClient(
    scoped_refptr<const Extension> extension)
    : extension_(extension) {
}

bool ExtensionZoomRequestClient::ShouldSuppressBubble() const {
  const Feature* feature =
      FeatureProvider::GetBehaviorFeature(behavior_feature::kZoomWithoutBubble);
  return feature && feature->IsAvailableToExtension(extension()).is_available();
}

ExtensionZoomRequestClient::~ExtensionZoomRequestClient() {
}

}  // namespace extensions
