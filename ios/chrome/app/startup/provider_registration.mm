// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/startup/provider_registration.h"

#include "base/check.h"
#include "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ProviderRegistration

+ (void)registerProviders {
  // Needs to happen before any function of the provider API is used.
  ios::provider::Initialize();

  std::unique_ptr<ios::ChromeBrowserProvider> provider =
      ios::CreateChromeBrowserProvider();

  // Leak the providers.
  ios::ChromeBrowserProvider* previous_provider =
      ios::SetChromeBrowserProvider(provider.release());

  DCHECK(!previous_provider)
      << "-registerProviders with an existing ChromeBrowserProvider registered";
}

@end
