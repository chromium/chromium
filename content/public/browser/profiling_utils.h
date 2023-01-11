// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PROFILING_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_PROFILING_UTILS_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

// Ask all the child processes to dump their profiling data to disk and calls
// |callback| once it's done.
CONTENT_EXPORT void AskAllChildrenToDumpProfilingData(
    base::OnceClosure callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PROFILING_UTILS_H_
