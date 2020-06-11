// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/dynamic_library_support.h"

#include <stdint.h>

#include "base/command_line.h"
#include "build/build_config.h"
#include "mojo/public/c/system/functions.h"

namespace mojo {

namespace {

// Helper for temporary storage related to |MojoInitialize()| calls.
struct InitializationState {
  InitializationState(const base::Optional<base::FilePath>& path,
                      MojoInitializeFlags flags) {
    options.flags = flags;

    if (path) {
      utf8_path = path->AsUTF8Unsafe();
      options.mojo_core_path = utf8_path.c_str();
      options.mojo_core_path_length = static_cast<uint32_t>(utf8_path.size());
    }

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
    // Build a temporary reconstructed argv to pass into the library so it can
    // inspect the application command line if needed.
    for (const std::string& s : base::CommandLine::ForCurrentProcess()->argv())
      argv.push_back(s.c_str());
    options.argc = static_cast<uint32_t>(argv.size());
    options.argv = argv.data();
#endif
  }

  MojoInitializeOptions options = {sizeof(MojoInitializeOptions)};
  std::string utf8_path;
  std::vector<const char*> argv;
};

}  // namespace

MojoResult LoadCoreLibrary(base::Optional<base::FilePath> path) {
  InitializationState state(path, MOJO_INITIALIZE_FLAG_LOAD_ONLY);
  return MojoInitialize(&state.options);
}

MojoResult InitializeCoreLibrary(MojoInitializeFlags flags) {
  DCHECK_EQ(flags & MOJO_INITIALIZE_FLAG_LOAD_ONLY, 0u);
  InitializationState state(base::nullopt, flags);
  return MojoInitialize(&state.options);
}

MojoResult LoadAndInitializeCoreLibrary(base::Optional<base::FilePath> path,
                                        MojoInitializeFlags flags) {
  DCHECK_EQ(flags & MOJO_INITIALIZE_FLAG_LOAD_ONLY, 0u);
  InitializationState state(path, flags);
  return MojoInitialize(&state.options);
}

}  // namespace mojo
