// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_PUBLIC_FEATURES_H_

// An enum representing the arms of the feature `kDefaultBrowserMagicStackIos`.
// Each arm determines where users are directed when they tap the Default
// Browser Magic Stack card.
enum class DefaultBrowserMagicStackIosVariationType {
  kDisabled,
  // The Default Browser card deep-links to iOS Settings.
  kTapToDeviceSettings,
  // The Default Browser card links to the "Default Browser" settings within the
  // app.
  kTapToAppSettings,
};

// Name of the parameter that controls the experiment type for
// `segmentation_platform::features::kDefaultBrowserMagicStackIos`.
extern const char kDefaultBrowserMagicStackIosVariation[];

// Returns which variation of
// `segmentation_platform::features::kDefaultBrowserMagicStackIos` is enabled or
// `kDisabled` if the feature is disabled.
DefaultBrowserMagicStackIosVariationType
GetDefaultBrowserMagicStackIosVariation();

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_DEFAULT_BROWSER_PUBLIC_FEATURES_H_
