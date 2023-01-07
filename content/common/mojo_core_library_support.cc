// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo_core_library_support.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

namespace content {

bool IsMojoCoreSharedLibraryEnabled() {
  return GetMojoCoreSharedLibraryPath() != absl::nullopt;
}

absl::optional<base::FilePath> GetMojoCoreSharedLibraryPath() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kMojoCoreLibraryPath))
    return absl::nullopt;
  return command_line.GetSwitchValuePath(switches::kMojoCoreLibraryPath);
#else
  // Content does not yet properly support dynamic Mojo Core on platforms other
  // than Linux and Chrome OS.
  return absl::nullopt;
#endif
}

}  // namespace content
