// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_
#define SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_

#include "base/files/file_path.h"
#include "services/screen_ai/buildflags/buildflags.h"

namespace screen_ai {

#if BUILDFLAG(ENABLE_SCREEN_AI_BROWSERTESTS)
// Get the absolute path of the ScreenAI component binary for tests. This
// function verifies that the binary exists on disk and can be opened.
base::FilePath GetComponentBinaryPathForTests();
#endif

// Returns the install directory relative to components folder.
base::FilePath GetRelativeInstallDir();

// Returns the folder in which ScreenAI component is installed.
base::FilePath GetComponentDir();

// Returns the file name of component binary.
base::FilePath GetComponentBinaryFileName();

// Returns the commandline switch for the binary file path.
const char* GetBinaryPathSwitch();

}  // namespace screen_ai
#endif  // SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_
