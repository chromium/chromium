// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/test/web_view_test_suite.h"

#include "ios/testing/screen_time_util.h"

namespace ios_web_view {

WebViewTestSuite::WebViewTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

WebViewTestSuite::~WebViewTestSuite() = default;

void WebViewTestSuite::Initialize() {
  base::TestSuite::Initialize();
  EnsureScreenTimeReady();
}

}  // namespace ios_web_view
