// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SPARSE_ATTRIBUTE_SETTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SPARSE_ATTRIBUTE_SETTER_H_

#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/aom/accessible_node_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class AXSparseAttributeSetter {
  USING_FAST_MALLOC(AXSparseAttributeSetter);

 public:
  virtual void Run(const AXObject&,
                   AXSparseAttributeClient&,
                   const AtomicString& value) = 0;
};

using AXSparseAttributeSetterMap =
    HashMap<QualifiedName, AXSparseAttributeSetter*>;

// A map from attribute name to a AXSparseAttributeSetter that
// calls AXSparseAttributeClient when that attribute's value
// changes.
//
// That way we only need to iterate over the list of attributes once,
// rather than calling getAttribute() once for each possible obscure
// accessibility attribute.
AXSparseAttributeSetterMap& GetSparseAttributeSetterMap();

// An implementation of AOMPropertyClient that calls
// AXSparseAttributeClient for an AOM property.
class AXSparseAttributeAOMPropertyClient : public AOMPropertyClient {
 public:
  AXSparseAttributeAOMPropertyClient(
      AXObjectCacheImpl& ax_object_cache,
      AXSparseAttributeClient& sparse_attribute_client)
      : ax_object_cache_(ax_object_cache),
        sparse_attribute_client_(sparse_attribute_client) {}

  void AddStringProperty(AOMStringProperty, const String& value) override;
  void AddBooleanProperty(AOMBooleanProperty, bool value) override;
  void AddIntProperty(AOMIntProperty, int32_t value) override;
  void AddUIntProperty(AOMUIntProperty, uint32_t value) override;
  void AddFloatProperty(AOMFloatProperty, float value) override;
  void AddRelationProperty(AOMRelationProperty,
                           const AccessibleNode& value) override;
  void AddRelationListProperty(AOMRelationListProperty,
                               const AccessibleNodeList& relations) override;

 private:
  Persistent<AXObjectCacheImpl> ax_object_cache_;
  AXSparseAttributeClient& sparse_attribute_client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SPARSE_ATTRIBUTE_SETTER_H_
