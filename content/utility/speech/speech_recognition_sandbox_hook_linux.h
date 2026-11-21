// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_SPEECH_SPEECH_RECOGNITION_SANDBOX_HOOK_LINUX_H_
#define CONTENT_UTILITY_SPEECH_SPEECH_RECOGNITION_SANDBOX_HOOK_LINUX_H_

#include <vector>

#include "base/files/file_path.h"
#include "sandbox/linux/syscall_broker/broker_file_permission.h"
#include "sandbox/policy/linux/sandbox_linux.h"

namespace speech {

// Returns the name of the command line switch used by the browser process to
// pass the path of the SODA binary to preload.
const char* GetSodaBinaryPathSwitch();

// Returns the broker file permissions required by the Speech On-Device API
// (SODA). When `allow_proc_cpuinfo` is true, read-only access to /proc/cpuinfo
// is included as a fallback for older SODA binaries without
// `SodaPreSandboxInit`. Exposed for testing.
std::vector<sandbox::syscall_broker::BrokerFilePermission>
GetSodaFilePermissions(bool allow_proc_cpuinfo);

// Calls the optional `SodaPreSandboxInit` export in `soda_library` before the
// sandbox is engaged. Returns true if the symbol is present and succeeded, or
// false if `soda_library` is null, the symbol is absent (older SODA binary), or
// initialization failed. Exposed for testing.
bool TrySodaPreSandboxInit(void* soda_library);

// Opens the libsoda.so binary at `binary_path`, runs its optional
// `SodaPreSandboxInit` hook before sandboxing, and grants broker file
// permissions to the necessary files required by the binary. `binary_path` is
// the path the browser process will ask this process to load; it must be
// opened before the sandbox is engaged, since libsoda.so's initializers need
// file access that the sandbox denies. If it is empty, the latest installed
// SODA binary is used instead.
bool SpeechRecognitionPreSandboxHook(
    base::FilePath binary_path,
    sandbox::policy::SandboxLinux::Options options);

}  // namespace speech

#endif  // CONTENT_UTILITY_SPEECH_SPEECH_RECOGNITION_SANDBOX_HOOK_LINUX_H_
