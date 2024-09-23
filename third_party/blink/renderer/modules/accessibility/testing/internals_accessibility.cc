// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/testing/internals_accessibility.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/testing/internals.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"

namespace blink {

unsigned InternalsAccessibility::numberOfLiveAXObjects(Internals&) {
  return AXObject::NumberOfLiveAXObjects();
}

namespace {
AXObject* GetAXObject(const Element* element) {
  Document& document = element->GetDocument();
  auto* ax_object_cache =
      To<AXObjectCacheImpl>(document.ExistingAXObjectCache());
  ax_object_cache->UpdateAXForAllDocuments();
  return ax_object_cache->Get(element);
}
}  // namespace

// static
WTF::String InternalsAccessibility::getComputedLabel(Internals&,
                                                     const Element* element) {
  AXObject* ax_object = GetAXObject(element);
  if (!ax_object || ax_object->IsIgnored()) {
    return g_empty_string;
  }

  ax::mojom::NameFrom name_from;
  AXObject::AXObjectVector name_objects;
  return ax_object->GetName(name_from, &name_objects);
}

// static
WTF::String InternalsAccessibility::getComputedRole(Internals&,
                                                    const Element* element) {
  AXObject* ax_object = GetAXObject(element);
  if (!ax_object || ax_object->IsIgnored()) {
    return AXObject::AriaRoleName(ax::mojom::Role::kNone);
  }

  ax::mojom::blink::Role role = ax_object->ComputeFinalRoleForSerialization();
  return AXObject::AriaRoleName(role);
}

}  // namespace blink
