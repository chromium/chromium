#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_INL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_INL_H_

// Implementation of ALWAYS_INLINE functions from ax_object.h and
// ax_node_object.h.

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_node_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"

namespace blink {

Node* AXObject::GetNode() const {
  if (auto* node_object = DynamicTo<AXNodeObject>(this)) {
    return node_object->GetNode();
  } else {
    return nullptr;
  }
}

Node* AXObject::GetClosestNode() const {
  return GetNode() ? GetNode() : ParentObject()->GetClosestNode();
}

Node* AXNodeObject::GetNode() const {
  if (IsDetached()) {
    DCHECK(!node_);
    return nullptr;
  }

  DCHECK(!GetLayoutObject() || GetLayoutObject()->GetNode() == node_)
      << "If there is an associated layout object, its node should match the "
         "associated node of this accessibility object.\n"
      << this;
  return node_;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_OBJECT_INL_H_
