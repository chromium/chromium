// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_SOURCE_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_SOURCE_HANDLE_H_

#include <compare>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;

// Lightweight identifier to element whose animated property contributes to a
// ComputedStyle value on that element itself, or a descendant.
class CORE_EXPORT AnimatedSourceHandle {
  DISALLOW_NEW();

 public:
  constexpr AnimatedSourceHandle() = default;

  // Creates a handle for the given element. Returns an invalid handle if null.
  // Note: May allocate through `DOMNodeIds::IdForNode()`.
  static AnimatedSourceHandle ForElement(Element* element);

  // Returns true if this handle references a valid element.
  constexpr bool IsValid() const { return animator_ != kInvalidDOMNodeId; }

  // Returns true if this handle matches the given element.
  // Note: Guaranteed not to allocate.
  bool IsOwnedBy(const Element& element) const;

  constexpr auto operator<=>(const AnimatedSourceHandle& other) const = default;

 private:
  constexpr explicit AnimatedSourceHandle(DOMNodeId animator)
      : animator_(animator) {}
  DOMNodeId animator_ = kInvalidDOMNodeId;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ANIMATED_SOURCE_HANDLE_H_
