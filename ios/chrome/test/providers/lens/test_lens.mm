// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <ostream>

#import "base/functional/bind.h"
#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {
namespace {

// The domain for NSErrors.
NSErrorDomain const kTestLensProviderErrorDomain =
    @"kTestLensProviderErrorDomain";

// The error codes for kTestLensProviderErrorDomain.
enum TestLensProviderErrors : NSInteger {
  kTestLensProviderErrorNotImplemented,
};

}

using LensWebParamsCallback =
    base::OnceCallback<void(web::NavigationManager::WebLoadParams)>;

id<ChromeLensController> NewChromeLensController(LensConfiguration* config) {
  // Lens is not supported for tests.
  return nil;
}

bool IsLensSupported() {
  // Lens is not supported for tests.
  return false;
}

bool IsLensWebResultsURL(const GURL& url) {
  // Lens is not supported for tests.
  return false;
}

absl::optional<LensEntrypoint> GetLensEntryPointFromURL(const GURL& url) {
  return absl::nullopt;
}

void GenerateLensLoadParamsAsync(LensQuery* query,
                                 LensWebParamsCallback completion) {
  NOTREACHED() << "Lens is not supported.";
}

}  // namespace provider
}  // namespace ios
