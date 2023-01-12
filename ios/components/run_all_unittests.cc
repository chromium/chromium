// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "ios/web/public/test/web_test_suite.h"
#include "mojo/core/embedder/embedder.h"

// Test suite for unit tests in ios/components.
class IOSComponentsUnitTestSuite : public web::WebTestSuite {
 public:
  IOSComponentsUnitTestSuite(int argc, char** argv);
};

IOSComponentsUnitTestSuite::IOSComponentsUnitTestSuite(int argc, char** argv)
    : web::WebTestSuite(argc, argv) {}

int main(int argc, char** argv) {
  IOSComponentsUnitTestSuite test_suite(argc, argv);
  mojo::core::Init();
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
