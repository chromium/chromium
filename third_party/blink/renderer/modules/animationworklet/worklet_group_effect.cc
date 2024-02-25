// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_group_effect.h"

namespace blink {

WorkletGroupEffect::WorkletGroupEffect(
    const Vector<std::optional<base::TimeDelta>>& local_times,
    const Vector<Timing>& timings,
    const Vector<Timing::NormalizedTiming>& normalized_timings) {
  DCHECK_GE(local_times.size(), 1u);
  DCHECK_EQ(local_times.size(), timings.size());
  DCHECK_EQ(local_times.size(), normalized_timings.size());

  effects_.ReserveInitialCapacity(timings.size());
  for (int i = 0; i < static_cast<int>(local_times.size()); i++) {
    effects_.push_back(MakeGarbageCollected<WorkletAnimationEffect>(
        local_times[i], timings[i], normalized_timings[i]));
  }
}

void WorkletGroupEffect::Trace(Visitor* visitor) const {
  visitor->Trace(effects_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
