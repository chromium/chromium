// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_MAIN_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_MAIN_H_

#include "remoting/host/host_export.h"

namespace remoting {

// The remote_open_url entry point exported from remoting_core.dll.
HOST_EXPORT int RemoteOpenUrlMain(int argc, char** argv);

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_MAIN_H_
