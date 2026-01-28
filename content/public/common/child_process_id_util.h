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
ToOriginatingProcess(ChildProcessId process);

// Converts a `content::ChildProcessId` to a `network::RendererProcess` for
// passing to network services.  This requires that the process is valid.
network::RendererProcess CONTENT_EXPORT
ToRendererProcess(ChildProcessId process);

// Converts a `network::RendererProcess` to a `content::ChildProcessId` for
// reading from network services.
ChildProcessId CONTENT_EXPORT
ToChildProcessId(network::RendererProcess process);

// Converts a deprecated int32_t process_id to a `network::OriginatingProcess`
// while all of the usages are ported.
// TODO(crbug.com/379869738) Remove unsafe usages.
network::OriginatingProcess CONTENT_EXPORT
ToOriginatingProcessUnsafe(int32_t process_id);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CHILD_PROCESS_ID_UTIL_H_
