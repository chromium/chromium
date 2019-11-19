/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/tree_ordered_map.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

TreeOrderedMap::TreeOrderedMap() = default;

#if DCHECK_IS_ON()
static int g_remove_scope_level = 0;

TreeOrderedMap::RemoveScope::RemoveScope() {
  g_remove_scope_level++;
}

TreeOrderedMap::RemoveScope::~RemoveScope() {
  DCHECK(g_remove_scope_level);
  g_remove_scope_level--;
}
#endif

inline bool KeyMatchesId(const AtomicString& key, const Element& element) {
  return element.GetIdAttribute() == key;
}

inline bool KeyMatchesMapName(const AtomicString& key, const Element& element) {
  auto* html_map_element = DynamicTo<HTMLMapElement>(element);
  return html_map_element && html_map_element->GetName() == key;
}

inline bool KeyMatchesSlotName(const AtomicString& key,
                               const Element& element) {
  auto* html_slot_element = DynamicTo<HTMLSlotElement>(element);
  return html_slot_element && html_slot_element->GetName() == key;
}

void TreeOrderedMap::Add(const AtomicString& key, Element& element) {
  DCHECK(key);

  Map::AddResult add_result =
      map_.insert(key, MakeGarbageCollected<MapEntry>(element));
  if (add_result.is_new_entry)
    return;

  Member<MapEntry>& entry = add_result.stored_value->value;
  DCHECK(entry->count);
  entry->element = nullptr;
  entry->count++;
  entry->ordered_list.clear();
}

void TreeOrderedMap::Remove(const AtomicString& key, Element& element) {
  DCHECK(key);

  Map::iterator it = map_.find(key);
  if (it == map_.end())
    return;

  Member<MapEntry>& entry = it->value;
  DCHECK(entry->count);
  if (entry->count == 1) {
    DCHECK(!entry->element || entry->element == element);
    map_.erase(it);
  } else {
    if (entry->element == element) {
      DCHECK(entry->ordered_list.IsEmpty() ||
             entry->ordered_list.front() == element);
      entry->element =
          entry->ordered_list.size() > 1 ? entry->ordered_list[1] : nullptr;
    }
    entry->count--;
    entry->ordered_list.clear();
  }
}

template <bool keyMatches(const AtomicString&, const Element&)>
inline Element* TreeOrderedMap::Get(const AtomicString& key,
                                    const TreeScope& scope) const {
  DCHECK(key);

  MapEntry* entry = map_.at(key);
  if (!entry)
    return nullptr;

  DCHECK(entry->count);
  if (entry->element)
    return entry->element;

  // Iterate to find the node that matches. Nothing will match iff an element
  // with children having duplicate IDs is being removed -- the tree traversal
  // will be over an updated tree not having that subtree. In all other cases,
  // a match is expected.
  for (Element& element : ElementTraversal::StartsAfter(scope.RootNode())) {
    if (!keyMatches(key, element))
      continue;
    entry->element = &element;
    return &element;
  }
// As get()/getElementById() can legitimately be called while handling element
// removals, allow failure iff we're in the scope of node removals.
#if DCHECK_IS_ON()
  DCHECK(g_remove_scope_level);
#endif
  return nullptr;
}

Element* TreeOrderedMap::GetElementById(const AtomicString& key,
                                        const TreeScope& scope) const {
  return Get<KeyMatchesId>(key, scope);
}

const HeapVector<Member<Element>>& TreeOrderedMap::GetAllElementsById(
    const AtomicString& key,
    const TreeScope& scope) const {
  DCHECK(key);
  DEFINE_STATIC_LOCAL(Persistent<HeapVector<Member<Element>>>, empty_vector,
                      (MakeGarbageCollected<HeapVector<Member<Element>>>()));

  Map::iterator it = map_.find(key);
  if (it == map_.end())
    return *empty_vector;

  Member<MapEntry>& entry = it->value;
  DCHECK(entry->count);

  if (entry->ordered_list.IsEmpty()) {
    entry->ordered_list.ReserveCapacity(entry->count);
    for (Element* element =
             entry->element ? entry->element.Get()
                            : ElementTraversal::FirstWithin(scope.RootNode());
         entry->ordered_list.size() < entry->count;
         element = ElementTraversal::Next(*element)) {
      DCHECK(element);
      if (!KeyMatchesId(key, *element))
        continue;
      entry->ordered_list.UncheckedAppend(element);
    }
    if (!entry->element)
      entry->element = entry->ordered_list.front();
  }

  return entry->ordered_list;
}

Element* TreeOrderedMap::GetElementByMapName(const AtomicString& key,
                                             const TreeScope& scope) const {
  return Get<KeyMatchesMapName>(key, scope);
}

// TODO(hayato): Template get<> by return type.
HTMLSlotElement* TreeOrderedMap::GetSlotByName(const AtomicString& key,
                                               const TreeScope& scope) const {
  if (Element* slot = Get<KeyMatchesSlotName>(key, scope))
    return To<HTMLSlotElement>(slot);
  return nullptr;
}

Element* TreeOrderedMap::GetCachedFirstElementWithoutAccessingNodeTree(
    const AtomicString& key) {
  MapEntry* entry = map_.at(key);
  if (!entry)
    return nullptr;
  DCHECK(entry->count);
  return entry->element;
}

void TreeOrderedMap::Trace(Visitor* visitor) {
  visitor->Trace(map_);
}

void TreeOrderedMap::MapEntry::Trace(Visitor* visitor) {
  visitor->Trace(element);
  visitor->Trace(ordered_list);
}

}  // namespace blink
