// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_switches.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// The file path to indicate if ash is ready for testing.
// The file should not be on the file system initially. After
// ash is ready for testing, the file will be created.
// This is used to communicate between launcher and runner process. In general
// you should not pass in this arg directly.
const char content::test::switches::kAshReadyFilePath[] = "ash-ready-file-path";

// Ash chrome user data dir path.
const char content::test::switches::kAshUserDataDir[] = "ash-user-data-dir";

// A dir to store all the pids of ash chrome created during tests.
// This is used to communicate between launcher and runner process. In general
// you should not pass in this arg directly.
const char content::test::switches::kAshProcessesDirPath[] =
    "ash-processes-dir-path";
#endif
