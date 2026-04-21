// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_MIGRATE_HOST_MAIN_H_
#define REMOTING_HOST_LINUX_MIGRATE_HOST_MAIN_H_

#include "remoting/base/remoting_export.h"

namespace remoting {

// The migrate_host entry point exported from remoting_core.so.
REMOTING_EXPORT int MigrateHostMain(int argc, char** argv);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_MIGRATE_HOST_MAIN_H_
