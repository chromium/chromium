// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_BROWSER_PROVIDER_H_
#define IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_BROWSER_PROVIDER_H_

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

class ChromiumBrowserProvider : public ios::ChromeBrowserProvider {
 public:
  ChromiumBrowserProvider();
  ~ChromiumBrowserProvider() override;

  // ChromeBrowserProvider implementation
  std::unique_ptr<ios::ChromeIdentityService> CreateChromeIdentityService()
      override;
};

#endif  // IOS_CHROME_BROWSER_PROVIDERS_CHROMIUM_BROWSER_PROVIDER_H_
