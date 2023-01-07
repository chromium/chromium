// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_MEMORY_INSTRUMENTATION_UTIL_H_
#define CONTENT_BROWSER_TRACING_MEMORY_INSTRUMENTATION_UTIL_H_

#include "content/common/content_export.h"

namespace content {

// Registers the browser process as a memory-instrumentation client, so that
// data for the browser process will be available in memory dumps.
void CONTENT_EXPORT InitializeBrowserMemoryInstrumentationClient();

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_MEMORY_INSTRUMENTATION_UTIL_H_
