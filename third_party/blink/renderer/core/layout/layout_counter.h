/*
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_COUNTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_COUNTER_H_

#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/style/counter_content.h"

namespace blink {

class CounterNode;
class PseudoElement;

using CounterMap = HashMap<AtomicString, scoped_refptr<CounterNode>>;

// LayoutCounter is used to represent the text of a counter.
// See http://www.w3.org/TR/CSS21/generate.html#counters
//
// Counters are always generated content ("content: counter(a)") thus this
// LayoutObject is always anonymous.
//
// CounterNodes is where the logic for knowing the value of a counter is.
// LayoutCounter makes sure the CounterNodes tree is consistent with the
// style. It then just queries CounterNodes for their values.
//
// CounterNodes are rare so they are stored in a map instead of growing
// LayoutObject. counterMaps() (in LayoutCounter.cpp) keeps the association
// between LayoutObject and CounterNodes. To avoid unneeded hash-lookups in the
// common case where there is no CounterNode, LayoutObject also keeps track of
// whether it has at least one CounterNode in the hasCounterNodeMap bit.
//
// Keeping the map up to date is the reason why LayoutObjects need to call into
// LayoutCounter during their lifetime (see the static functions below).
class LayoutCounter final : public LayoutText {
 public:
  LayoutCounter(PseudoElement&, const CounterContent&);
  ~LayoutCounter() override;

  // These functions are static so that any LayoutObject can call them.
  // The reason is that any LayoutObject in the tree can have a CounterNode
  // without a LayoutCounter (e.g. by specifying 'counter-increment' without
  // a "content: counter(a)" directive)).
  static void DestroyCounterNodes(LayoutObject&);
  static void DestroyCounterNode(LayoutObject&, const AtomicString& identifier);
  static void LayoutObjectSubtreeAttached(LayoutObject*);
  static void LayoutObjectSubtreeWillBeDetached(LayoutObject*);
  static void LayoutObjectStyleChanged(LayoutObject&,
                                       const ComputedStyle* old_style,
                                       const ComputedStyle& new_style);

  static CounterMap* GetCounterMap(LayoutObject*);

  void UpdateCounter();

  const char* GetName() const override { return "LayoutCounter"; }

 protected:
  void WillBeDestroyed() override;

 private:
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectCounter || LayoutText::IsOfType(type);
  }
  scoped_refptr<StringImpl> OriginalText() const override;

  // Removes the reference to the CounterNode associated with this layoutObject.
  // This is used to cause a counter display update when the CounterNode tree
  // changes.
  void Invalidate();

  CounterContent counter_;
  CounterNode* counter_node_;
  LayoutCounter* next_for_same_counter_;
  friend class CounterNode;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutCounter, IsCounter());

}  // namespace blink

#if DCHECK_IS_ON()
// Outside the blink namespace for ease of invocation from gdb.
void showCounterLayoutTree(const blink::LayoutObject*, const char* counterName);
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_COUNTER_H_
