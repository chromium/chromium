/*
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/counter_node.h"

#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

#if DCHECK_IS_ON()
#include <stdio.h>
#endif

namespace blink {

void CounterNode::Trace(Visitor* visitor) const {
  visitor->Trace(owner_);
  visitor->Trace(scope_);
  visitor->Trace(previous_in_parent_);
}

Element& CounterNode::OwnerElement() const {
  LayoutObject* owner = owner_;
  while (owner && !IsA<Element>(owner->GetNode())) {
    owner = owner->Parent();
  }
  CHECK(owner && IsA<Element>(owner->GetNode()));
  return *To<Element>(owner->GetNode());
}

Element& CounterNode::OwnerNonPseudoElement() const {
  Element& element = OwnerElement();
  if (element.IsPseudoElement()) {
    return *element.ParentOrShadowHostElement();
  }
  return element;
}

AtomicString CounterNode::DebugName() const {
  AtomicString counter_type = AtomicString(
      HasUseType()
          ? "USE"
          : (HasResetType() ? "RESET" : (HasSetType() ? "SET" : "INC")));
  String counter_name =
      !OwnerElement().IsPseudoElement()
          ? OwnerElement().DebugName()
          : OwnerElement().ParentOrShadowHostElement()->DebugName() +
                OwnerElement().DebugName();
  AtomicString result = counter_type + " AT " + counter_name;
  return result;
}

}  // namespace blink
