// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file should contain test switches for non unit gtests. Like
// browser test or interactive ui test.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TEST_SWITCHES_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TEST_SWITCHES_H_

#include "build/chromeos_buildflags.h"

namespace content::test::switches {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
extern const char kAshReadyFilePath[];
extern const char kAshUserDataDir[];
extern const char kAshProcessesDirPath[];
#endif

}  // namespace content::test::switches

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TEST_SWITCHES_H_
