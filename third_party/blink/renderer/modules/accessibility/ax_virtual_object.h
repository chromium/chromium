// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_VIRTUAL_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_VIRTUAL_OBJECT_H_

#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class AXObjectCacheImpl;

class MODULES_EXPORT AXVirtualObject : public AXObject {
 public:
  AXVirtualObject(AXObjectCacheImpl&, AccessibleNode*);
  ~AXVirtualObject() override;
  void Trace(Visitor*) const override;

  // AXObject overrides.
  void Detach() override;
  bool IsVirtualObject() const override { return true; }
  void AddChildren() override;
  const AtomicString& GetAOMPropertyOrARIAAttribute(
      AOMStringProperty) const override;
  bool HasAOMPropertyOrARIAAttribute(AOMBooleanProperty,
                                     bool& result) const override;
  AccessibleNode* GetAccessibleNode() const override;
  String TextAlternative(bool recursive,
                         const AXObject* aria_label_or_description_root,
                         AXObjectSet& visited,
                         ax::mojom::NameFrom&,
                         AXRelatedObjectVector*,
                         NameSources*) const override;
  Document* GetDocument() const override;
  ax::mojom::blink::Role DetermineAccessibilityRole() override;
  ax::mojom::blink::Role NativeRoleIgnoringAria() const override;
  ax::mojom::blink::Role AriaRoleAttribute() const override;

 private:
  Member<AccessibleNode> accessible_node_;

  ax::mojom::blink::Role aria_role_;
};

template <>
struct DowncastTraits<AXVirtualObject> {
  static bool AllowFrom(const AXObject& object) {
    return object.IsVirtualObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_VIRTUAL_OBJECT_H_
