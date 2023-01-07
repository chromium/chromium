// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Main entry point for all unit tests.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "rlz/lib/rlz_lib.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "rlz/lib/rlz_value_store.h"
#endif

int main(int argc, char **argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  testing::InitGoogleMock(&argc, argv);
  testing::InitGoogleTest(&argc, argv);

  mojo::core::Init();

  // RlzLibTest uses base::test::TaskEnvironment that needs TestTimeouts.
  TestTimeouts::Initialize();

  int ret = RUN_ALL_TESTS();
  if (ret == 0) {
    // Now re-run all the tests using a supplementary brand code.  This brand
    // code will remain in effect for the lifetime of the branding object.
#if BUILDFLAG(IS_POSIX)
    // Set a temporary directory for RLZ here, because SupplementaryBranding
    // creates and owns RlzValueStore object for its lifetime.
    base::ScopedTempDir temp_dir;
    if (temp_dir.CreateUniqueTempDir())
      rlz_lib::testing::SetRlzStoreDirectory(temp_dir.GetPath());
#endif
    rlz_lib::SupplementaryBranding branding("TEST");
    ret = RUN_ALL_TESTS();
  }

  return ret;
}
