// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_USERNAME_H_
#define REMOTING_BASE_USERNAME_H_

#include <string>

#include "base/strings/cstring_view.h"
#include "build/build_config.h"

namespace remoting {

// Returns the username associated with this process, or the empty string on
// error or if not implemented.
std::string GetUsername();

#if BUILDFLAG(IS_LINUX)
// Returns the username that the network process is run as.
base::cstring_view GetNetworkProcessUsername();
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace remoting

#endif  // REMOTING_BASE_USERNAME_H_
