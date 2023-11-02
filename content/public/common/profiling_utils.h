// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PROFILING_UTILS_H_
#define CONTENT_PUBLIC_COMMON_PROFILING_UTILS_H_

#include "base/files/file.h"
#include "content/common/content_export.h"

namespace content {

// Open the file that should be used by a child process to save its profiling
// data.
CONTENT_EXPORT base::File OpenProfilingFile();

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_PROFILING_UTILS_H_
