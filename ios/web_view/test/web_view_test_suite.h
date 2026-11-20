// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_SUITE_H_
#define IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_SUITE_H_

#include "base/test/test_suite.h"

namespace ios_web_view {

// Test suite shared by ios_web_view_unittests and ios_web_view_inttests. Waits
// for ScreenTime to be ready before running tests, so the first web view loads
// don't hang on cold booted simulators.
class WebViewTestSuite : public base::TestSuite {
 public:
  WebViewTestSuite(int argc, char** argv);
  ~WebViewTestSuite() override;

 protected:
  // base::TestSuite:
  void Initialize() override;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_TEST_WEB_VIEW_TEST_SUITE_H_
