// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_

#include <memory>

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

namespace ios {

class TestChromeBrowserProvider : public ChromeBrowserProvider {
 public:
  TestChromeBrowserProvider();
  ~TestChromeBrowserProvider() override;

  TestChromeBrowserProvider(const TestChromeBrowserProvider&) = delete;
  TestChromeBrowserProvider& operator=(const TestChromeBrowserProvider&) =
      delete;

  // Returns the current provider as a `TestChromeBrowserProvider`.
  static TestChromeBrowserProvider& GetTestProvider();

 private:
  // ChromeBrowserProvider:
  std::unique_ptr<ChromeIdentityService> CreateChromeIdentityService() override;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_
