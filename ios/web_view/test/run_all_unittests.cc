// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "ios/web_view/test/web_view_test_suite.h"
#include "mojo/core/embedder/embedder.h"

int main(int argc, char** argv) {
  ios_web_view::WebViewTestSuite test_suite(argc, argv);
  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&ios_web_view::WebViewTestSuite::Run,
                     base::Unretained(&test_suite)));
}
