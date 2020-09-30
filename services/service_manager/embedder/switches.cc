// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/embedder/switches.h"

namespace service_manager {
namespace switches {

#if defined(OS_WIN)

// Prefetch arguments are used by the Windows prefetcher to disambiguate
// different execution modes (i.e. process types) of the same executable image
// so that different types of processes don't trample each others' prefetch
// behavior.
//
// Legal values are integers in the range [1, 8]. We reserve 8 to mean
// "whatever", and this will ultimately lead to processes with /prefetch:8
// having inconsistent behavior thus disabling prefetch in practice.
//
// TODO(rockot): Make it possible for embedders to override this argument on a
// per-service basis.
const char kDefaultServicePrefetchArgument[] = "/prefetch:8";

#endif  // defined(OS_WIN)

// Disables the in-process stack traces.
const char kDisableInProcessStackTraces[] = "disable-in-process-stack-traces";

// Controls whether console logging is enabled and optionally configures where
// it's routed.
const char kEnableLogging[] = "enable-logging";

// Indicates the type of process to run. This may be "service-manager",
// "service-runner", or any other arbitrary value supported by the embedder.
const char kProcessType[] = "type";

// The token to use to construct the message pipe for a service in a child
// process.
const char kServiceRequestChannelToken[] = "service-request-channel-token";

// Describes the file descriptors passed to a child process in the following
// list format:
//
//     <file_id>:<descriptor_id>,<file_id>:<descriptor_id>,...
//
// where <file_id> is an ID string from the manifest of the service being
// launched and <descriptor_id> is the numeric identifier of the descriptor for
// the child process can use to retrieve the file descriptor from the
// global descriptor table.
const char kSharedFiles[] = "shared-files";

// Causes the process to run as a zygote.
const char kZygoteProcess[] = "zygote";

}  // namespace switches
}  // namespace service_manager
