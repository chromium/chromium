// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "remoting/host/host_main.h"

#if BUILDFLAG(IS_APPLE)
// Required by WindowServer on macOS for pre-login LaunchAgents running in the
// LoginWindow session. Declared in `entry_point.cc` while the hybrid binary
// serves both single-process and multi-process desktop modes, and so component
// builds carry the section in the main executable rather than
// `libremoting_core.dylib`.
// TODO(crbug.com/499082867): Move to desktop_process_main.cc once
// single-process support is dropped.
__attribute__((used))
__attribute__((section("__CGPreLoginApp,__cgpreloginapp"))) static const char
    kMagicPreLoginSection[] = "";
#endif  // BUILDFLAG(IS_APPLE)

int main(int argc, char** argv) {
  return remoting::HostMain(argc, argv);
}
