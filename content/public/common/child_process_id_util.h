// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CHILD_PROCESS_ID_UTIL_H_
#define CONTENT_PUBLIC_COMMON_CHILD_PROCESS_ID_UTIL_H_

#include "content/common/content_export.h"
#include "content/public/common/child_process_id.h"
#include "services/network/public/cpp/originating_process.h"

namespace content {

// Converts a `content::ChildProcessId` to a `network::OriginatingProcess` for
// passing to network services.
network::OriginatingProcess CONTENT_EXPORT
ToOriginatingProcess(const ChildProcessId& process);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CHILD_PROCESS_ID_UTIL_H_
