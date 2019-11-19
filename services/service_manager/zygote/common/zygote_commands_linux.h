// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SERVICE_MANAGER_ZYGOTE_COMMON_ZYGOTE_COMMANDS_LINUX_H_
#define SERVICES_SERVICE_MANAGER_ZYGOTE_COMMON_ZYGOTE_COMMANDS_LINUX_H_

#include <stddef.h>

#include "base/posix/global_descriptors.h"

namespace service_manager {

// Contents of the initial message sent from the zygote to the browser right
// after it starts.
static const char kZygoteBootMessage[] = "ZYGOTE_BOOT";

// Contents of the initial message sent from the zygote to the browser when it
// is ready to go.
static const char kZygoteHelloMessage[] = "ZYGOTE_OK";

// Message sent by zygote children to the browser so the browser can discover
// the sending child's process ID.
static const char kZygoteChildPingMessage[] = "CHILD_PING";

// Maximum allowable length for messages sent to the zygote.
const size_t kZygoteMaxMessageLength = 12288;

// File descriptors initialized by the Zygote Host
const int kZygoteSocketPairFd = base::GlobalDescriptors::kBaseDescriptor;

// These are the command codes used on the wire between the browser and the
// zygote.
enum {
  // Fork off a new renderer.
  kZygoteCommandFork = 0,

  // Reap a renderer child.
  kZygoteCommandReap = 1,

  // Check what happened to a child process.
  kZygoteCommandGetTerminationStatus = 2,

  // Read a bitmask of kSandboxLinux*
  kZygoteCommandGetSandboxStatus = 3,

  // Not a real zygote command, but a subcommand used during the zygote fork
  // protocol.  Sends the child's PID as seen from the browser process.
  kZygoteCommandForkRealPID = 4
};

}  // namespace service_manager

#endif  // SERVICES_SERVICE_MANAGER_ZYGOTE_COMMON_ZYGOTE_COMMANDS_LINUX_H_
