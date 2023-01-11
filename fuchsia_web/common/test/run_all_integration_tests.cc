// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/system/sys_info.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"

constexpr size_t kDefaultTestBatchLimit = 10;

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  // Each test launches a complete set of components, so allow two CPU cores
  // per test.
  size_t jobs = base::SysInfo::NumberOfProcessors();
  if (jobs > 1)
    jobs /= 2;

  return base::LaunchUnitTestsWithOptions(
      argc, argv, jobs, kDefaultTestBatchLimit, true /* use_job_objects */,
      base::DoNothing(),
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
