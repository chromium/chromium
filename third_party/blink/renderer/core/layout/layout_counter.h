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

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class CounterNode;
class PseudoElement;

using CounterMap = HeapHashMap<AtomicString, Member<CounterNode>>;

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
// LayoutObject. GetCounterMaps() (in layout_counter.cc) keeps the association
// between LayoutObject and CounterNodes. To avoid unneeded hash-lookups in the
// common case where there is no CounterNode, LayoutObject also keeps track of
// whether it has at least one CounterNode in the HasCounterNodeMap bit.
//
// Keeping the map up to date is the reason why LayoutObjects need to call into
// LayoutCounter during their lifetime (see the static functions below).
class LayoutCounter : public LayoutText {
 public:
  LayoutCounter(PseudoElement&, const CounterContentData&);
  ~LayoutCounter() override;
  void Trace(Visitor*) const override;

  const AtomicString& Identifier() const {
    NOT_DESTROYED();
    return counter_->Identifier();
  }
  void SetCounterNode(CounterNode* counter_node) {
    NOT_DESTROYED();
    counter_node_ = counter_node;
  }
  CounterNode* GetCounterNode() const {
    NOT_DESTROYED();
    return counter_node_;
  }

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

  // Returns true if <counter-style> is "disclosure-open" or
  // "disclosure-closed".
  bool IsDirectionalSymbolMarker() const;
  // Returns <string> in counters().
  const AtomicString& Separator() const;

  // Returns LayoutCounter::counter_->ListStyle() if `object` is a
  // LayoutCounter.
  // Returns style.ListStyleType()->GetCounterStyleName() otherwise.
  static const AtomicString& ListStyle(const LayoutObject* object,
                                       const ComputedStyle& style);

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutCounter";
  }

 protected:
  void WillBeDestroyed() override;

 private:
  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectCounter || LayoutText::IsOfType(type);
  }
  String OriginalText() const override;

  // Removes the reference to the CounterNode associated with this layoutObject.
  // This is used to cause a counter display update when the CounterNode tree
  // changes.
  void Invalidate();

  const CounterStyle* NullableCounterStyle() const;

  Member<const CounterContentData> counter_;
  Member<CounterNode> counter_node_;
  Member<LayoutCounter> next_for_same_counter_;
  friend class CounterNode;
};

template <>
struct DowncastTraits<LayoutCounter> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsCounter();
  }
};

}  // namespace blink

#if DCHECK_IS_ON()
// Outside the blink namespace for ease of invocation from gdb.
void ShowCounterLayoutTree(const blink::LayoutObject*, const char* counterName);
#endif

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_COUNTER_H_
