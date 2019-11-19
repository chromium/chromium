// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_ID_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

namespace probe {

// The core probes use this class as an identifier for an async task.
class CORE_EXPORT AsyncTaskId {
 public:
  void SetAdTask() { ad_task_ = true; }
  bool IsAdTask() const { return ad_task_; }

 private:
  bool ad_task_ = false;
};

}  // namespace probe

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_ID_H_
