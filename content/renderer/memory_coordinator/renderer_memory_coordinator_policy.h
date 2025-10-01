// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEMORY_COORDINATOR_RENDERER_MEMORY_COORDINATOR_POLICY_H_
#define CONTENT_RENDERER_MEMORY_COORDINATOR_RENDERER_MEMORY_COORDINATOR_POLICY_H_

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/field_trial_params.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"

namespace content {

CONTENT_EXPORT BASE_DECLARE_FEATURE(kMemoryCoordinatorLastResortGC);
CONTENT_EXPORT BASE_DECLARE_FEATURE_PARAM(int, kRestoreLimitSeconds);

class ChildMemoryConsumerRegistry;

// The main policy that lives in renderer processes. Acts on memory consumers
// in a single renderer, based on signals that comes from inside the renderer.
class CONTENT_EXPORT RendererMemoryCoordinatorPolicy {
 public:
  // Returns the global instance instance. This will CHECK that the instance
  // exists.
  static RendererMemoryCoordinatorPolicy& Get();

  explicit RendererMemoryCoordinatorPolicy(
      ChildMemoryConsumerRegistry& registry);
  ~RendererMemoryCoordinatorPolicy();

  // Notifies the policy that V8 is about to run its last resort GC.
  void OnV8HeapLastResortGC();

 private:
  void OnRestoreLimitTimerFired();

  // The registry outlives `this`.
  raw_ref<ChildMemoryConsumerRegistry> registry_;

  base::OneShotTimer restore_limit_timer_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEMORY_COORDINATOR_RENDERER_MEMORY_COORDINATOR_POLICY_H_
