// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SPECULATION_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SPECULATION_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/timing/preload_data.h"
#include "third_party/blink/renderer/core/timing/speculation_navigation_data.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// https://github.com/WICG/speculative_load_measurement
class SpeculationData final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SpeculationData(HeapVector<Member<PreloadData>> preloads,
                  HeapVector<Member<SpeculationNavigationData>> navigations);

  const HeapVector<Member<PreloadData>>& preloads() const { return preloads_; }
  const HeapVector<Member<SpeculationNavigationData>>& navigations() const {
    return navigations_;
  }

  void Trace(Visitor* visitor) const override;

 private:
  HeapVector<Member<PreloadData>> preloads_;
  HeapVector<Member<SpeculationNavigationData>> navigations_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_SPECULATION_DATA_H_
