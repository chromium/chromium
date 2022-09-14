// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "ios/web/public/navigation/navigation_manager.h"

#import "base/bind.h"
#import "base/notreached.h"
#import "base/threading/sequenced_task_runner_handle.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

}

id<ChromeLensController> NewChromeLensController(LensConfiguration* config) {
  // Lens is not supported in Chromium.
  return nil;
}

bool IsLensSupported() {
  // Lens is not supported in Chromium.
  return false;
}

web::NavigationManager::WebLoadParams GenerateLensLoadParamsForImage(
    UIImage* image,
    LensEntrypoint entry_point,
    bool is_incognito) {
  // Lens is not supported in Chromium; this function will never be called.
  NOTREACHED() << "Lens is not supported.";
  return web::NavigationManager::WebLoadParams({});
}

}  // namespace provider
}  // namespace ios
