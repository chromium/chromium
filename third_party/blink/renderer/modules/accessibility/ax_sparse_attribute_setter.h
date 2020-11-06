// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SPARSE_ATTRIBUTE_SETTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SPARSE_ATTRIBUTE_SETTER_H_

#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/aom/accessible_node_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/accessibility/ax_node_data.h"

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
// TODO(meredithl): Migrate this to the temp setter for crbug/1068668
AXSparseAttributeSetterMap& GetSparseAttributeSetterMap();

// A map from attribute name to a callback that sets the |value| for that
// attribute on an AXNodeData. This is designed to replace the above sparse
// attribute setter. This name is temporary, the above name of
// AXSparseAttributeSetterMap will be used once all sparse attributes are
// migrated.
using AXSparseSetterFunc =
    base::RepeatingCallback<void(AXObject* ax_object,
                                 ui::AXNodeData* node_data,
                                 const AtomicString& value)>;
using TempSetterMap = HashMap<QualifiedName, AXSparseSetterFunc>;

TempSetterMap& GetTempSetterMap();

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
  void AddFloatProperty(AOMFloatProperty, float value) override;
  void AddRelationProperty(AOMRelationProperty,
                           const AccessibleNode& value) override;
  void AddRelationListProperty(AOMRelationListProperty,
                               const AccessibleNodeList& relations) override;

 private:
  Persistent<AXObjectCacheImpl> ax_object_cache_;
  AXSparseAttributeClient& sparse_attribute_client_;
};

class AXNodeDataAOMPropertyClient : public AOMPropertyClient {
 public:
  AXNodeDataAOMPropertyClient(AXObjectCacheImpl& ax_object_cache,
                              ui::AXNodeData& node_data)
      : ax_object_cache_(ax_object_cache), node_data_(node_data) {}

  void AddStringProperty(AOMStringProperty, const String& value) override;
  void AddBooleanProperty(AOMBooleanProperty, bool value) override;
  void AddFloatProperty(AOMFloatProperty, float value) override;
  void AddRelationProperty(AOMRelationProperty,
                           const AccessibleNode& value) override;
  void AddRelationListProperty(AOMRelationListProperty,
                               const AccessibleNodeList& relations) override;

 private:
  Persistent<AXObjectCacheImpl> ax_object_cache_;
  ui::AXNodeData& node_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SPARSE_ATTRIBUTE_SETTER_H_
