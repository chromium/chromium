// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SECCOMP_SANDBOX_STATUS_ANDROID_H_
#define CONTENT_RENDERER_SECCOMP_SANDBOX_STATUS_ANDROID_H_

#include "sandbox/linux/seccomp-bpf-helpers/seccomp_starter_android.h"

namespace content {

void SetSeccompSandboxStatus(sandbox::SeccompSandboxStatus status);

}  // namespace content

#endif  // CONTENT_RENDERER_SECCOMP_SANDBOX_STATUS_ANDROID_H_
