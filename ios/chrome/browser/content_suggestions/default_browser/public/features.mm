// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/default_browser/public/features.h"

#import "components/segmentation_platform/public/features.h"

namespace {
BASE_FEATURE_PARAM(
    int,
    kDefaultBrowserMagicStackIosVariationFeature,
    &segmentation_platform::features::kDefaultBrowserMagicStackIos,
    kDefaultBrowserMagicStackIosVariation,
    1);
}  // namespace

const char kDefaultBrowserMagicStackIosVariation[] =
    "DefaultBrowserMagicStackIosVariation";

DefaultBrowserMagicStackIosVariationType
GetDefaultBrowserMagicStackIosVariation() {
  if (!base::FeatureList::IsEnabled(
          segmentation_platform::features::kDefaultBrowserMagicStackIos)) {
    return DefaultBrowserMagicStackIosVariationType::kDisabled;
  }

  return static_cast<DefaultBrowserMagicStackIosVariationType>(
      kDefaultBrowserMagicStackIosVariationFeature.Get());
}
