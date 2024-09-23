// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/renderer/controller/tests/thread_state_test_environment.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

int main(int argc, char** argv) {
  ::testing::AddGlobalTestEnvironment(new content::BlinkTestEnvironment);
  ::testing::AddGlobalTestEnvironment(new ThreadStateTestEnvironment);

  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
