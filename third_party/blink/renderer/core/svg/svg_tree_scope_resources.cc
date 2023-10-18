// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_tree_scope_resources.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/svg/svg_resource.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

SVGTreeScopeResources::SVGTreeScopeResources(TreeScope* tree_scope)
    : tree_scope_(tree_scope) {}

LocalSVGResource* SVGTreeScopeResources::ResourceForId(const AtomicString& id) {
  if (id.empty())
    return nullptr;
  auto& entry = resources_.insert(id, nullptr).stored_value->value;
  if (!entry)
    entry = MakeGarbageCollected<LocalSVGResource>(*tree_scope_, id);
  return entry.Get();
}

LocalSVGResource* SVGTreeScopeResources::ExistingResourceForId(
    const AtomicString& id) const {
  if (id.empty())
    return nullptr;
  auto it = resources_.find(id);
  if (it == resources_.end())
    return nullptr;
  return it->value.Get();
}

void SVGTreeScopeResources::ProcessCustomWeakness(const LivenessBroker& info) {
  // Unregister and remove any resources that are no longer alive.
  Vector<AtomicString> to_remove;
  for (auto& resource_entry : resources_) {
    if (info.IsHeapObjectAlive(resource_entry.value))
      continue;
    resource_entry.value->Unregister();
    to_remove.push_back(resource_entry.key);
  }
  resources_.RemoveAll(to_remove);
}

void SVGTreeScopeResources::Trace(Visitor* visitor) const {
  visitor->template RegisterWeakCallbackMethod<
      SVGTreeScopeResources, &SVGTreeScopeResources::ProcessCustomWeakness>(
      this);
  visitor->Trace(resources_);
  visitor->Trace(tree_scope_);
}

}  // namespace blink
