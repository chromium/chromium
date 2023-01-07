// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
std::unique_ptr<ChromeBrowserProvider> CreateChromeBrowserProvider() {
  return std::make_unique<TestChromeBrowserProvider>();
}
}  // namespace ios
