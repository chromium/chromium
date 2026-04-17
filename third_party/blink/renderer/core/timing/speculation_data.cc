// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/speculation_data.h"

namespace blink {

SpeculationData::SpeculationData(HeapVector<Member<PreloadData>> preloads)
    : preloads_(std::move(preloads)) {}

void SpeculationData::Trace(Visitor* visitor) const {
  visitor->Trace(preloads_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
