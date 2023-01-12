// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"

int main(int argc, char** argv) {
  int result = 0;
  {
    base::TestSuite test_suite(argc, argv);
    mojo::core::Init();
    result = base::LaunchUnitTests(
        argc, argv,
        base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
  }
  return result;
}
