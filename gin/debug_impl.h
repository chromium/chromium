// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_DEBUG_IMPL_H_
#define GIN_PUBLIC_DEBUG_IMPL_H_

#include "gin/public/debug.h"
#include "v8/include/v8.h"

namespace gin {

class DebugImpl {
 public:
  static v8::JitCodeEventHandler GetJitCodeEventHandler();
};

}  // namespace gin

#endif  // GIN_PUBLIC_DEBUG_IMPL_H_
