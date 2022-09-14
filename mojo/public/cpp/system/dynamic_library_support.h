// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_DYNAMIC_LIBRARY_SUPPORT_H_
#define MOJO_PUBLIC_CPP_SYSTEM_DYNAMIC_LIBRARY_SUPPORT_H_

#include "base/files/file_path.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/system_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

// Helper to load Mojo Core dynamically from a shared library. If |path| is
// not given, the library path is assumed to be set in the
// MOJO_CORE_LIBRARY_PATH environment variable, or the library is searched for
// in the current working directory.
//
// This may only be called in a process that hasn't already initialized Mojo.
// Mojo is still not fully initialized or usable until |InitializeCoreLibrary()|
// is also called. These two functions are kept distinct to facilitate use
// cases where the client application must perform some work (e.g. sandbox
// configuration, forking, etc) between the loading and initialization steps.
MOJO_CPP_SYSTEM_EXPORT MojoResult
LoadCoreLibrary(absl::optional<base::FilePath> path);

// Initializes the dynamic Mojo Core library previously loaded by
// |LoadCoreLibrary()| above.
//
// This may only be called in a process that hasn't already initialized Mojo.
MOJO_CPP_SYSTEM_EXPORT MojoResult
InitializeCoreLibrary(MojoInitializeFlags flags);

// Loads and initializes Mojo Core from a shared library. This combines
// |LoadCoreLibrary()| and |InitializeCoreLibrary()| for convenience in cases
// where they don't need to be performed at different times by the client
// application.
MOJO_CPP_SYSTEM_EXPORT MojoResult
LoadAndInitializeCoreLibrary(absl::optional<base::FilePath> path,
                             MojoInitializeFlags flags);

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_DYNAMIC_LIBRARY_SUPPORT_H_
