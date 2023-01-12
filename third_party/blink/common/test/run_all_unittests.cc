// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/test/run_all_unittests.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "v8/include/libplatform/libplatform.h"
#include "v8/include/v8.h"

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  mojo::core::Init();
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  auto platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
