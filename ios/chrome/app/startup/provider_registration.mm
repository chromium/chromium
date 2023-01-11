// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/provider_registration.h"

#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ProviderRegistration

+ (void)registerProviders {
  // Needs to happen before any function of the provider API is used.
  ios::provider::Initialize();
}

@end
