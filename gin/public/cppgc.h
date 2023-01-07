// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_CPPGC_H_
#define GIN_PUBLIC_CPPGC_H_

#include "gin/gin_export.h"

namespace gin {

// A wrapper around `cppgc::InitializeProcess()` which helps to guarantee that
// cppgc is initialized only once when there are multiple users of cppgc in same
// process.
GIN_EXPORT void InitializeCppgcFromV8Platform();

// Calls `cppgc::ShutdownProcess()` only after being called as many times as
// `InitializeCppgcFromV8Platform()`. Helps to guarantee that cppgc is shutdown
// only after all users in the same process are done using it. Number of calls
// cannot exceed that of `InitializeCppgcFromV8Platform()`.
GIN_EXPORT void MaybeShutdownCppgc();

}  // namespace gin

#endif  // GIN_PUBLIC_CPPGC_H_
