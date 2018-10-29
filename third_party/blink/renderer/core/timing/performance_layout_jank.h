// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_LAYOUT_JANK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_LAYOUT_JANK_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"

namespace blink {

// The PerformanceLayoutJank object exposes the jank fraction of an animation
// frame that does not occur close to some user input. The jank fraction
// approximates the fraction of the viewport affected by layout jank during that
// frame. More details can be found in this explainer:
// http://bit.ly/lsm-explainer.
class CORE_EXPORT PerformanceLayoutJank final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PerformanceLayoutJank* Create(double fraction);

  ~PerformanceLayoutJank() override;

  AtomicString entryType() const override;
  PerformanceEntryType EntryTypeEnum() const override;

  double fraction() const { return fraction_; }

  void Trace(blink::Visitor*) override;

 private:
  PerformanceLayoutJank(double fraction);

  void BuildJSONValue(V8ObjectBuilder&) const override;

  double fraction_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_LAYOUT_JANK_H_
