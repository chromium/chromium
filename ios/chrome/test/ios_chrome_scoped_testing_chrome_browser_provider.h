// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_CHROME_BROWSER_PROVIDER_H_
#define IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_CHROME_BROWSER_PROVIDER_H_

#include <memory>

#include "base/macros.h"

namespace ios {
class ChromeBrowserProvider;
}

// Helper class to temporarily set up a ChromeBrowserProvider and restore
// the previous one on teardown.
class IOSChromeScopedTestingChromeBrowserProvider {
 public:
  explicit IOSChromeScopedTestingChromeBrowserProvider(
      std::unique_ptr<ios::ChromeBrowserProvider> chrome_browser_provider);
  ~IOSChromeScopedTestingChromeBrowserProvider();

 private:
  std::unique_ptr<ios::ChromeBrowserProvider> chrome_browser_provider_;
  ios::ChromeBrowserProvider* original_chrome_browser_provider_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeScopedTestingChromeBrowserProvider);
};

#endif  // IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_CHROME_BROWSER_PROVIDER_H_
