// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

// Implementation of the PartRoot class, which is part of the DOM Parts API.
// A PartRoot can exist for a |Document| or |DocumentFragment|, and this the
// entrypoint for the |getParts()| interface, which queries for contained parts.
class CORE_EXPORT PartRoot : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PartRoot(const PartRoot&) = delete;
  ~PartRoot() override = default;

  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
  }

  // PartRoot API

  // TODO(crbug.com/1453291) Implement this method.
  HeapVector<Member<Part>> getParts() { return HeapVector<Member<Part>>(); }

 protected:
  PartRoot() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_
