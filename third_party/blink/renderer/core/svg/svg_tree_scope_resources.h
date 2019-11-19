// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_TREE_SCOPE_RESOURCES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_TREE_SCOPE_RESOURCES_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class LocalSVGResource;
class TreeScope;

// This class keeps track of SVG resources and pending references to such for a
// TreeScope. It's per-TreeScope because that matches the lookup scope of an
// element's id (which is used to identify a resource.)
class SVGTreeScopeResources final
    : public GarbageCollected<SVGTreeScopeResources> {
 public:
  explicit SVGTreeScopeResources(TreeScope*);
  ~SVGTreeScopeResources();

  LocalSVGResource* ResourceForId(const AtomicString& id);
  LocalSVGResource* ExistingResourceForId(const AtomicString& id) const;

  void Trace(Visitor*);

 private:
  void ProcessCustomWeakness(const WeakCallbackInfo&);

  HeapHashMap<AtomicString, WeakMember<LocalSVGResource>> resources_;
  Member<TreeScope> tree_scope_;

  DISALLOW_COPY_AND_ASSIGN(SVGTreeScopeResources);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_TREE_SCOPE_RESOURCES_H_
