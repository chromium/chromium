// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"

int wmain(int argc, wchar_t **argv) {
  if (argc >= 2) {
    if (0 == _wcsicmp(argv[1], L"-child"))
      // This instance is a child, not the test.
      return 0;
  }

  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv, false,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
