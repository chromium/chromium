// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/seccomp_sandbox_status_android.h"

#include "content/public/renderer/seccomp_sandbox_status_android.h"

namespace content {

static sandbox::SeccompSandboxStatus g_status =
    sandbox::SeccompSandboxStatus::NOT_SUPPORTED;

void SetSeccompSandboxStatus(sandbox::SeccompSandboxStatus status) {
  g_status = status;
}

sandbox::SeccompSandboxStatus GetSeccompSandboxStatus() {
  return g_status;
}

}  // namespace content
