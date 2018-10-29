// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_WINDOW_TASK_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_WINDOW_TASK_WORKLET_H_

#include "third_party/blink/renderer/core/workers/experimental/task_worklet.h"

namespace blink {

class WindowTaskWorklet {
  STATIC_ONLY(WindowTaskWorklet);

 public:
  static TaskWorklet* taskWorklet(LocalDOMWindow& window) {
    return TaskWorklet::From(window);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_WORKERS_EXPERIMENTAL_WINDOW_TASK_WORKLET_H_
