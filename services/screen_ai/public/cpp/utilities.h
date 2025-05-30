// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_
#define SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_

#include <stdint.h>

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

// Returns the maximum dimension for which images are processed without
// downsampling. This value is not expected to change after initialization of
// the service and is expected to be non-zero.
// This value is best to be retrieved through
// `ScreenAIAnnotator::GetMaxImageDimension` but as long the library does not
// have dynamic maximum resolution setting, it is safe to use this value.
uint32_t GetMaxDimensionForOCR();

}  // namespace screen_ai
#endif  // SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_
