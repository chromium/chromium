// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_DEBUG_IMPL_H_
#define GIN_DEBUG_IMPL_H_

#include "gin/public/debug.h"
#include "v8/include/v8-callbacks.h"

namespace gin {

class DebugImpl {
 public:
  static v8::JitCodeEventHandler GetJitCodeEventHandler();
};

}  // namespace gin

#endif  // GIN_DEBUG_IMPL_H_
