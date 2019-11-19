// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_side_navigation_test_utils.h"

#include "base/command_line.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/test/test_navigation_url_loader_factory.h"
#include "net/base/net_errors.h"

namespace content {

namespace {

// A UI thread singleton helper class for browser side navigations. When browser
// side navigations are enabled, initialize this class before doing any
// operation that may start a navigation request on the UI thread. Use TearDown
// at the end of the test.
class BrowserSideNavigationTestUtils {
 public:
  BrowserSideNavigationTestUtils()
      : loader_factory_(new TestNavigationURLLoaderFactory) {}

  ~BrowserSideNavigationTestUtils() {}

 private:
  std::unique_ptr<TestNavigationURLLoaderFactory> loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSideNavigationTestUtils);
};

base::LazyInstance<std::unique_ptr<BrowserSideNavigationTestUtils>>::
    DestructorAtExit browser_side_navigation_test_utils;

}  // namespace

void BrowserSideNavigationSetUp() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_side_navigation_test_utils.Get().reset(
      new BrowserSideNavigationTestUtils);
}

void BrowserSideNavigationTearDown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_side_navigation_test_utils.Get().reset(nullptr);
}

}  // namespace content
