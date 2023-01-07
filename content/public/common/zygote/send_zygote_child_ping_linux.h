// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ZYGOTE_SEND_ZYGOTE_CHILD_PING_LINUX_H_
#define CONTENT_PUBLIC_COMMON_ZYGOTE_SEND_ZYGOTE_CHILD_PING_LINUX_H_

#include "content/common/content_export.h"

namespace content {

// Sends a zygote child "ping" message to browser process via socket |fd|.
// Returns true on success.
CONTENT_EXPORT bool SendZygoteChildPing(int fd);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ZYGOTE_SEND_ZYGOTE_CHILD_PING_LINUX_H_
