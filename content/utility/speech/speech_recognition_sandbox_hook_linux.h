// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_SPEECH_SPEECH_RECOGNITION_SANDBOX_HOOK_LINUX_H_
#define CONTENT_UTILITY_SPEECH_SPEECH_RECOGNITION_SANDBOX_HOOK_LINUX_H_

#include "base/files/file_path.h"
#include "sandbox/policy/linux/sandbox_linux.h"

namespace speech {

// Returns the name of the command line switch used by the browser process to
// pass the path of the SODA binary to preload.
const char* GetSodaBinaryPathSwitch();

// Opens the libsoda.so binary at `binary_path` and grants broker file
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
