// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_part_root.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Part;

// Implementation of the PartRoot class, which is part of the DOM Parts API.
// PartRoot is the base of the class hierarchy.
class CORE_EXPORT PartRoot : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PartRoot() override = default;

  void Trace(Visitor* visitor) const override;

  // Adds a new part to this PartRoot's collection of maintained parts.
  void addPart(Part& new_part);

  // PartRoot API
  HeapVector<Member<Part>> getParts();
  // TODO(crbug.com/1453291) Implement this method.
  PartRoot* clone() const { return nullptr; }

 protected:
  PartRoot() = default;

 private:
  HeapVector<Member<Part>> parts_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_
