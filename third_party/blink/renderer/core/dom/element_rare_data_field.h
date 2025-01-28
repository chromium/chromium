// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_FIELD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_FIELD_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class Node;

class ElementRareDataField : public GarbageCollectedMixin {
 public:
  void Trace(Visitor* visitor) const override {}
};

template <typename T>
struct ThreadingTrait<
    T,
    std::enable_if_t<std::is_base_of_v<blink::ElementRareDataField, T> &&
                     !std::is_base_of_v<blink::Node, T>>> {
  static constexpr ThreadAffinity kAffinity = kMainThreadOnly;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ELEMENT_RARE_DATA_FIELD_H_
