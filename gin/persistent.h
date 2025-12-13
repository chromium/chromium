// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PERSISTENT_H_
#define GIN_PERSISTENT_H_

#include "v8/include/cppgc/persistent.h"
#include "v8/include/cppgc/source-location.h"

namespace gin {

// Wraps a raw pointer into a cppgc::Persistent. This is a simplified version
// of blink::WrapPersistent, which doesn't support location tracking.
template <typename T>
cppgc::Persistent<T> WrapPersistent(
    T* value,
    const cppgc::SourceLocation& loc = cppgc::SourceLocation()) {
  return cppgc::Persistent<T>(value, loc);
}

}  // namespace gin

#endif  // GIN_PERSISTENT_H_
