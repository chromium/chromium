// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/speculation_data.h"

namespace blink {

SpeculationData::SpeculationData(
    HeapVector<Member<PreloadData>> preloads,
    HeapVector<Member<SpeculationNavigationData>> navigations)
    : preloads_(std::move(preloads)), navigations_(std::move(navigations)) {}

void SpeculationData::Trace(Visitor* visitor) const {
  visitor->Trace(preloads_);
  visitor->Trace(navigations_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
