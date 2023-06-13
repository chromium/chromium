// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_part_root.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// Implementation of the PartRoot class, which is part of the DOM Parts API.
// A PartRoot adds getParts to Part.
class CORE_EXPORT PartRoot : public Part {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PartRoot() = default;
  PartRoot(const PartRoot&) = delete;
  ~PartRoot() override = default;

  // PartRoot API
  virtual HeapVector<Member<Part>> getParts() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_
