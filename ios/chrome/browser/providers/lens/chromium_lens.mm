// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import <ostream>

#import "base/functional/bind.h"
#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_api.h"
#import "ios/web/public/navigation/navigation_manager.h"

namespace ios {
namespace provider {
namespace {

// The domain for NSErrors.
NSErrorDomain const kChromiumLensProviderErrorDomain =
    @"kChromiumLensProviderErrorDomain";

// The error codes for kChromiumLensProviderErrorDomain.
enum ChromiumLensProviderErrors : NSInteger {
  kChromiumLensProviderErrorNotImplemented,
};

}  // namespace

UIViewController<ChromeLensViewFinderController>*
NewChromeLensViewFinderController(LensConfiguration* config) {
  // Lens is not supported in Chromium.
  return nil;
}

UIViewController<ChromeLensOverlay>* NewChromeLensOverlay(
    LensImageSource* imageSource,
    LensConfiguration* config,
    NSArray<UIAction*>* precedingMenuItems,
    NSArray<UIAction*>* additionalMenuItems) {
  // Lens is not supported in Chromium.
  return nil;
}

UIViewController<ChromeLensOverlay>* NewChromeLensOverlay(
    LensImageSource* imageSource,
    LensConfiguration* config,
    NSArray<UIAction*>* additionalMenuItems) {
  // Lens is not supported in Chromium.
  return nil;
}

bool IsLensSupported() {
  // Lens is not supported in Chromium.
  return false;
}

}  // namespace provider
}  // namespace ios
