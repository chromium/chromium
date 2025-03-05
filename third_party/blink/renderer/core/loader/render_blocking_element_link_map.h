// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RENDER_BLOCKING_ELEMENT_LINK_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RENDER_BLOCKING_ELEMENT_LINK_MAP_H_

#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// Tracks the currently pending render-blocking element ids and the links that
// caused them to be blocking.
// For instance,
//   <link id="link1" rel="expect" href="#target-id1" blocking="render"/>
//   <link id="link2" rel="expect" href="#target-id1" blocking="render"/>
//   <link id="link3" rel="expect" href="#target-id2" blocking="render"/>
// will be stored as the map:
// {
//  "target-id1":{link1, link2}
//  "target-id2":{link3}
// }.
class RenderBlockingElementLinkMap
    : public GarbageCollected<RenderBlockingElementLinkMap> {
 public:
  explicit RenderBlockingElementLinkMap(
      base::RepeatingClosure blocking_elmenents_empty_handler);

  // Read-only iteration through all the links with an active render-blocking
  // element.
  void ForEach(base::RepeatingCallback<void(const HTMLLinkElement&)> func);

  void AddLinkWithTargetElement(const AtomicString& target_element_id,
                                const HTMLLinkElement* link);
  void RemoveTargetElement(const AtomicString& target_element_id);
  void RemoveLinkWithTargetElement(const AtomicString& target_element_id,
                                   const HTMLLinkElement* link);
  bool HasElement() const { return !element_render_blocking_links_.empty(); }
  void Clear();

  void Trace(Visitor* visitor) const;

  ~RenderBlockingElementLinkMap();

 private:
  HeapHashMap<AtomicString,
              Member<HeapHashSet<WeakMember<const HTMLLinkElement>>>>
      element_render_blocking_links_;
  base::RepeatingClosure on_blocking_elements_empty_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RENDER_BLOCKING_ELEMENT_LINK_MAP_H_
