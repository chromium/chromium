// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/test_chrome_provider_initializer.h"

#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

TestChromeProviderInitializer::TestChromeProviderInitializer() {
  chrome_browser_provider_.reset(new TestChromeBrowserProvider());
  ChromeBrowserProvider* previous_provider =
      ios::SetChromeBrowserProvider(chrome_browser_provider_.get());
  EXPECT_FALSE(previous_provider);
}

TestChromeProviderInitializer::~TestChromeProviderInitializer() {
  ChromeBrowserProvider* previous_provider =
      ios::SetChromeBrowserProvider(nullptr);
  EXPECT_EQ(previous_provider, chrome_browser_provider_.get());
}

}  // namespace ios
