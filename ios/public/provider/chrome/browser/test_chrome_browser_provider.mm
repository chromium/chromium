// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

TestChromeBrowserProvider::TestChromeBrowserProvider() {}

TestChromeBrowserProvider::~TestChromeBrowserProvider() {}

// static
TestChromeBrowserProvider& TestChromeBrowserProvider::GetTestProvider() {
  ChromeBrowserProvider& provider = GetChromeBrowserProvider();
  return static_cast<TestChromeBrowserProvider&>(provider);
}

std::unique_ptr<ChromeIdentityService>
TestChromeBrowserProvider::CreateChromeIdentityService() {
  return std::make_unique<FakeChromeIdentityService>();
}

}  // namespace ios
