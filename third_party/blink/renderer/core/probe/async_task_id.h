// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_ID_H_

#include <cstdint>

#include "base/optional.h"
#include "base/trace_event/trace_id_helper.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

namespace probe {

// The core probes use this class as an identifier for an async task.
class CORE_EXPORT AsyncTaskId {
 public:
  AsyncTaskId() = default;

  // Not copyable or movable, since the address of an AsyncTaskId is what's used
  // to keep track of them.
  AsyncTaskId(const AsyncTaskId&) = delete;
  AsyncTaskId& operator=(const AsyncTaskId&) = delete;

  void SetAdTask() { ad_task_ = true; }
  bool IsAdTask() const { return ad_task_; }

  // Trace id for this task.
  base::Optional<uint64_t> GetTraceId() const { return trace_id_; }
  void SetTraceId(uint64_t trace_id) { trace_id_ = trace_id; }

 private:
  bool ad_task_ = false;
  base::Optional<uint64_t> trace_id_;
};

}  // namespace probe

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PROBE_ASYNC_TASK_ID_H_
