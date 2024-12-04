// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_ID_H_
#define CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_ID_H_

#include "base/types/id_type.h"
#include "content/public/common/content_constants.h"

namespace content {
// An opaque ID type used to uniquely identify child process instances. This
// is separate from system PID. Values are never reused.
// This uses kInvalidChildProcessUniqueId (-1) as the default invalid id,
// but also recognizes 0 as an invalid id because there is existing code that
// uses 0 as an invalid value. It starts generating id's at 1.
using ChildProcessId = base::IdType<class ChildProcessIdTag,
                                    int32_t,
                                    kInvalidChildProcessUniqueId,
                                    /*kFirstGeneratedId=*/1,
                                    /*kExtraInvalidValues=*/0>;
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHILD_PROCESS_ID_H_
