// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo_core_library_support.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

namespace content {

bool IsMojoCoreSharedLibraryEnabled() {
  return GetMojoCoreSharedLibraryPath() != base::nullopt;
}

base::Optional<base::FilePath> GetMojoCoreSharedLibraryPath() {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kMojoCoreLibraryPath))
    return base::nullopt;
  return command_line.GetSwitchValuePath(switches::kMojoCoreLibraryPath);
#else
  // Content does not yet properly support dynamic Mojo Core on platforms other
  // than Linux and Chrome OS.
  return base::nullopt;
#endif
}

}  // namespace content
