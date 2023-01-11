// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_BRANDING_H_
#define REMOTING_HOST_BRANDING_H_

#include "base/files/file_path.h"
#include "build/build_config.h"

namespace remoting {

#if BUILDFLAG(IS_WIN)
// Windows chromoting service name.
extern const wchar_t kWindowsServiceName[];
#endif

// Returns the location of the host configuration directory.
base::FilePath GetConfigDir();

}  // namespace remoting

#endif  // REMOTING_HOST_BRANDING_H_
