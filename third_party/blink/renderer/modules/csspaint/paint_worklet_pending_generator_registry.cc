// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_pending_generator_registry.h"

#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"

namespace blink {

void PaintWorkletPendingGeneratorRegistry::NotifyGeneratorReady(
    const String& name) {
  auto it = pending_generators_.find(name);
  if (it != pending_generators_.end()) {
    GeneratorHashSet* set = it->value;
    for (const auto& generator : *set) {
      if (generator)
        generator->NotifyGeneratorReady();
    }
  }
  pending_generators_.erase(name);
}

void PaintWorkletPendingGeneratorRegistry::AddPendingGenerator(
    const String& name,
    CSSPaintImageGeneratorImpl* generator) {
  Member<GeneratorHashSet>& set =
      pending_generators_.insert(name, nullptr).stored_value->value;
  if (!set)
    set = MakeGarbageCollected<GeneratorHashSet>();
  set->insert(generator);
}

void PaintWorkletPendingGeneratorRegistry::Trace(Visitor* visitor) const {
  visitor->Trace(pending_generators_);
}

}  // namespace blink
