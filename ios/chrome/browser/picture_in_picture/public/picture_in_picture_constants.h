// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_PUBLIC_PICTURE_IN_PICTURE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_PUBLIC_PICTURE_IN_PICTURE_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import <string_view>

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Represents the feature for which the configuration is created.
// LINT.IfChange(PictureInPictureFeature)
enum class PictureInPictureFeature : NSInteger {
  // Default browser.
  kDefaultBrowser,
  // Must be last for UMA histogram.
  kMaxValue = kDefaultBrowser,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:PictureInPictureFeature)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Represents the app restoration from the picture in picture session.
// LINT.IfChange(PictureInPictureAppRestoration)
enum class PictureInPictureAppRestoration : NSInteger {
  // Manual restoration (user restores the app from the home screen or app
  // switcher).
  kManual,
  // Fullscreen restoration (user taps the fullscreen button in the picture in
  // picture view).
  kPictureInPictureFullscreenButton,
  // Must be last for UMA histogram.
  kMaxValue = kPictureInPictureFullscreenButton,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:PictureInPictureAppRestoration)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Represents the picture in picture session start.
// LINT.IfChange(PictureInPictureDismissalReason)
enum class PictureInPictureDismissalReason : NSInteger {
  // Picture in picture not supported.
  kNotSupported,
  // Manual app restoration.
  kManualAppRestoration,
  // In the app, close the fullscreen video player.
  kInAppCloseButton,
  // In the app, swipe to close the fullscreen video player.
  kInAppSwipeToClose,
  // In the picture in picture view, close the video player.
  kPictureInPictureCloseButton,
  // Must be last for UMA histogram.
  kMaxValue = kPictureInPictureCloseButton,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:PictureInPictureDismissalReason)

// Returns a string representation of the given PictureInPictureFeature.
std::string_view PictureInPictureFeatureToString(
    PictureInPictureFeature feature);

#endif  // IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_PUBLIC_PICTURE_IN_PICTURE_CONSTANTS_H_
