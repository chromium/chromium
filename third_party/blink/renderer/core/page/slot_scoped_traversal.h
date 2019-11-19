// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SLOT_SCOPED_TRAVERSAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SLOT_SCOPED_TRAVERSAL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;
class HTMLSlotElement;

class CORE_EXPORT SlotScopedTraversal {
  STATIC_ONLY(SlotScopedTraversal);

 public:
  static HTMLSlotElement* FindScopeOwnerSlot(const Element&);
  static Element* NearestInclusiveAncestorAssignedToSlot(const Element&);
  static Element* Next(const Element&);
  static Element* Previous(const Element&);
  static Element* FirstAssignedToSlot(HTMLSlotElement&);
  static Element* LastAssignedToSlot(HTMLSlotElement&);

  static bool IsSlotScoped(const Element&);
};

}  // namespace blink

#endif
