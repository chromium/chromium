// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_SECCOMP_SANDBOX_STATUS_ANDROID_H_
#define CONTENT_PUBLIC_RENDERER_SECCOMP_SANDBOX_STATUS_ANDROID_H_

#include "content/common/content_export.h"

#include "sandbox/linux/seccomp-bpf-helpers/seccomp_starter_android.h"

namespace content {

// Gets the SeccompSandboxStatus of the current process.
CONTENT_EXPORT sandbox::SeccompSandboxStatus GetSeccompSandboxStatus();

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_SECCOMP_SANDBOX_STATUS_ANDROID_H_
