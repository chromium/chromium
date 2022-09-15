// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_SANDBOX_HELPER_WIN_H_
#define CONTENT_PUBLIC_APP_SANDBOX_HELPER_WIN_H_

namespace sandbox {
struct SandboxInterfaceInfo;
}

namespace content {

// Initializes the sandbox code and turns on DEP. Note: This function
// must be *statically* linked into the executable (along with the static
// sandbox library); it will not work correctly if it is exported from a
// DLL and linked in.
void InitializeSandboxInfo(sandbox::SandboxInterfaceInfo* sandbox_info);

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_SANDBOX_HELPER_WIN_H_
