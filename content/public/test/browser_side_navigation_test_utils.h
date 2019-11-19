// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_SIDE_NAVIGATION_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BROWSER_SIDE_NAVIGATION_TEST_UTILS_H_

#include <memory>

#include "base/macros.h"

namespace content {

// Initializes the browser side navigation test utils. Following this call, all
// NavigationURLLoader objects created will be TestNavigationURLLoaders instead
// of NavigationURLloaderImpls. This should be called before any call in the UI
// thread unit tests that will start a navigation (eg.
// TestWebContents::NavigateAndCommit).
void BrowserSideNavigationSetUp();

// Tears down the browser side navigation test utils.
void BrowserSideNavigationTearDown();

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_SIDE_NAVIGATION_TEST_UTILS_H_
