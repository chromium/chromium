// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_MAIN_H_
#define REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_MAIN_H_

#include "remoting/host/host_export.h"

namespace remoting {

// The me2me native messaging host entry point exported from remoting_core.dll.
HOST_EXPORT int Me2MeNativeMessagingHostMain(int argc, char** argv);

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_ME2ME_NATIVE_MESSAGING_HOST_MAIN_H_
