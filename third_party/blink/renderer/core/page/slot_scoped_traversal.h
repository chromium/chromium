// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SLOT_SCOPED_TRAVERSAL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SLOT_SCOPED_TRAVERSAL_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Element;
class HTMLSlotElement;

// TODO(http://crbug.com/1439837): Rename this class as it no longer provides
// traversal functionality.
class CORE_EXPORT SlotScopedTraversal {
  STATIC_ONLY(SlotScopedTraversal);

 public:
  static HTMLSlotElement* FindScopeOwnerSlot(const Element&);
  static Element* NearestInclusiveAncestorAssignedToSlot(const Element&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SLOT_SCOPED_TRAVERSAL_H_
