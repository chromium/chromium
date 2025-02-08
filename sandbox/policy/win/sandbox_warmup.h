// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_WIN_SANDBOX_WARMUP_H_
#define SANDBOX_POLICY_WIN_SANDBOX_WARMUP_H_

#include "sandbox/policy/export.h"

namespace sandbox::policy {

// Call in a sandboxed process before target lockdown where modules should be
// pre-loaded to support the infrastructure underlying crypto::RandBytes.
SANDBOX_POLICY_EXPORT void WarmupRandomnessInfrastructure();

// Call in a sandboxed process where dwrite.dll might be loaded and Csrss
// lockdown makes GetUserDefaultLCID() crash in its tracing infrastructure.
// Returns `true` if the hooks are emplaced, `false` if dwrite is not loaded,
// or the hooks could not be emplaced. If emplaced, the hooks are never removed.
SANDBOX_POLICY_EXPORT bool HookDwriteGetUserDefaultLCID();

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_WIN_SANDBOX_WARMUP_H_
