// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SPARSE_ATTRIBUTE_SETTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SPARSE_ATTRIBUTE_SETTER_H_

#include "base/memory/raw_ref.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/aom/accessible_node_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/accessibility/ax_node_data.h"

namespace blink {

// A map from attribute name to a callback that sets the |value| for that
// attribute on an AXNodeData.
//
// That way we only need to iterate over the list of attributes once,
// rather than calling getAttribute() once for each possible obscure
// accessibility attribute.

using AXSparseSetterFunc =
    base::RepeatingCallback<void(AXObject* ax_object,
                                 ui::AXNodeData* node_data,
                                 const AtomicString& value)>;
using AXSparseAttributeSetterMap = HashMap<QualifiedName, AXSparseSetterFunc>;

AXSparseAttributeSetterMap& GetAXSparseAttributeSetterMap();

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
  const raw_ref<ui::AXNodeData, ExperimentalRenderer> node_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_SPARSE_ATTRIBUTE_SETTER_H_
