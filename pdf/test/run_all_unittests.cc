// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "third_party/blink/public/platform/platform.h"

namespace {

class PdfTestSuite final : public base::TestSuite {
 public:
  using TestSuite::TestSuite;
  PdfTestSuite(const PdfTestSuite&) = delete;
  PdfTestSuite& operator=(const PdfTestSuite&) = delete;

 protected:
  void Initialize() override {
    TestSuite::Initialize();
    blink::Platform::InitializeBlink();
  }
};

}  // namespace

int main(int argc, char** argv) {
  PdfTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&PdfTestSuite::Run, base::Unretained(&test_suite)));
}
