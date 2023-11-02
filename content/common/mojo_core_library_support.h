// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_CORE_LIBRARY_SUPPORT_H_
#define CONTENT_COMMON_MOJO_CORE_LIBRARY_SUPPORT_H_

#include "base/files/file_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Indicates whether the calling process was launched with the option to
// initialize Mojo Core from a shared library rather than the statically linked
// implementation.
bool IsMojoCoreSharedLibraryEnabled();

// Returns the path to the Mojo Core shared library passed in on the command
// line for the calling process, or null if the process was launched without a
// Mojo Core library path on the command line.
absl::optional<base::FilePath> GetMojoCoreSharedLibraryPath();

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_CORE_LIBRARY_SUPPORT_H_
