// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MOJO_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MOJO_HELPER_H_

#include "base/task/current_thread.h"

namespace blink {

// Used to get whether message loop is ready for current thread, to help
// blink::initialize() determining whether can initialize mojo stuff or not.
// TODO(leonhsl): http://crbug.com/660274 Remove this API by ensuring
// a message loop before calling blink::initialize().
inline bool CanInitializeMojo() {
  return base::CurrentThread::IsSet();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_MOJO_HELPER_H_
