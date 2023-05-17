// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CRASH_CRASH_UPLOADER_MAIN_H_
#define REMOTING_HOST_CRASH_CRASH_UPLOADER_MAIN_H_

#include "remoting/host/host_export.h"

namespace remoting {

// The remoting_crash_uploader entry point exported from libremoting_core.so
HOST_EXPORT int CrashUploaderMain(int argc, char** argv);

}  // namespace remoting

#endif  // REMOTING_HOST_CRASH_CRASH_UPLOADER_MAIN_H_
