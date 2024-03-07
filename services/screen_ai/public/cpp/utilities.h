// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_
#define SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_

#include "base/files/file_path.h"

namespace screen_ai {

// Returns the absolute path of the highest-versioned component directory.
base::FilePath GetLatestComponentPath();

// Get the absolute path of the ScreenAI component. This function verifies that
// the binary exists on disk and can be opened. It may return empty if some
// other piece of software has opened the binary file with restrictive
// permissions (e.g., "security" software or malware protection).
base::FilePath GetLatestComponentBinaryPath();

// Returns the install directory relative to components folder.
base::FilePath GetRelativeInstallDir();

// Returns the folder in which ScreenAI component is installed.
base::FilePath GetComponentDir();

// Returns the file name of component binary.
base::FilePath GetComponentBinaryFileName();

}  // namespace screen_ai
#endif  // SERVICES_SCREEN_AI_PUBLIC_CPP_UTILITIES_H_
