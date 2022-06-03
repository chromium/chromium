// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/test/unittest_test_suite.h"
#include "content/test/content_test_suite.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "content/common/set_process_title_linux.h"
#endif

int main(int argc, char** argv) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // For setproctitle unit tests.
  setproctitle_init(const_cast<const char**>(argv));
#endif

  content::UnitTestTestSuite test_suite(
      new content::ContentTestSuite(argc, argv));
  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&content::UnitTestTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
