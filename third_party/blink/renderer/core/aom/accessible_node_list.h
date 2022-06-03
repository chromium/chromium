// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_AOM_ACCESSIBLE_NODE_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_AOM_ACCESSIBLE_NODE_LIST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

class AccessibleNode;
enum class AOMRelationListProperty;
class ExceptionState;

// Accessibility Object Model node list
// Explainer: https://github.com/WICG/aom/blob/master/explainer.md
// Spec: https://wicg.github.io/aom/spec/
class CORE_EXPORT AccessibleNodeList : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AccessibleNodeList* Create(const HeapVector<Member<AccessibleNode>>&);

  AccessibleNodeList();
  ~AccessibleNodeList() override;

  void AddOwner(AOMRelationListProperty, AccessibleNode*);
  void RemoveOwner(AOMRelationListProperty, AccessibleNode*);

  AccessibleNode* item(unsigned offset) const;
  void add(AccessibleNode*, AccessibleNode* = nullptr);
  void remove(int index);
  IndexedPropertySetterResult AnonymousIndexedSetter(unsigned,
                                                     AccessibleNode*,
                                                     ExceptionState&);
  unsigned length() const;
  void setLength(unsigned);

  void Trace(Visitor*) const override;

 private:
  void NotifyChanged();

  HeapVector<std::pair<AOMRelationListProperty, Member<AccessibleNode>>>
      owners_;
  HeapVector<Member<AccessibleNode>> nodes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_AOM_ACCESSIBLE_NODE_LIST_H_
