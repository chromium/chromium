// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ZYGOTE_SANDBOX_SUPPORT_LINUX_H_
#define CONTENT_PUBLIC_COMMON_ZYGOTE_SANDBOX_SUPPORT_LINUX_H_

#include <stddef.h>

#include "content/common/content_export.h"

namespace content {

// Gets the well-known file descriptor on which we expect to find the
// sandbox IPC channel.
CONTENT_EXPORT int GetSandboxFD();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ZYGOTE_SANDBOX_SUPPORT_LINUX_H_
