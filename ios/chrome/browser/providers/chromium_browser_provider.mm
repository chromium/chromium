// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/chromium_browser_provider.h"

#import <memory>

#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumBrowserProvider::ChromiumBrowserProvider() {}

ChromiumBrowserProvider::~ChromiumBrowserProvider() {}

std::unique_ptr<ios::ChromeIdentityService>
ChromiumBrowserProvider::CreateChromeIdentityService() {
  return std::make_unique<ios::ChromeIdentityService>();
}
