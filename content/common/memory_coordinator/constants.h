// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_CONSTANTS_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_CONSTANTS_H_

#include <stddef.h>

namespace content {

// The maximum number of memory consumers a single child process is allowed to
// register. This limit ensures a child process doesn't send more messages than
// reasonable to the browser process.
inline constexpr size_t kMaxMemoryConsumersPerProcess = 200u;

// The maximum allowed character length for a memory consumer's name. This
// ensures a child process doesn't send overly large strings to the browser
// process.
inline constexpr size_t kMaxMemoryConsumerNameLength = 100u;

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_CONSTANTS_H_
