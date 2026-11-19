// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_WORKLET_PERFORMANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_WORKLET_PERFORMANCE_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// https://w3c.github.io/hr-time/#sec-workletperformance
//
// Unlike Performance, this is not an EventTarget and owns no timeline state;
// it only holds the values now() needs so that it can be called from the
// rendering thread without allocating.
class CORE_EXPORT WorkletPerformance final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  WorkletPerformance(base::TimeTicks time_origin,
                     bool has_cross_origin_isolated_capability);

  DOMHighResTimeStamp now() const;

 private:
  const base::TimeTicks time_origin_;
  const bool has_cross_origin_isolated_capability_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_WORKLET_PERFORMANCE_H_
