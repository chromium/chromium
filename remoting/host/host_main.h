// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_HOST_MAIN_H_
#define REMOTING_HOST_HOST_MAIN_H_

#include "remoting/host/host_export.h"

namespace remoting {

// The common entry point exported from remoting_core.dll. It uses
// "--type==<type>" command line parameter to determine the kind of process it
// needs to run.
HOST_EXPORT int HostMain(int argc, char** argv);

}  // namespace remoting

#endif  // REMOTING_HOST_HOST_MAIN_H_
