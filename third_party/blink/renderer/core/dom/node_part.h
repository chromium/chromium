// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_PART_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_PART_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_node_part_init.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;

// Implementation of the NodePart class, which is part of the DOM Parts API.
// A NodePart stores a reference to a single |Node| in the DOM tree.
class CORE_EXPORT NodePart : public Part {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NodePart* Create(PartRoot* root,
                          Node* node,
                          const NodePartInit* init,
                          ExceptionState& exception_state);
  // TODO(crbug.com/1453291): Handle the init parameter.
  NodePart(PartRoot& root, Node* node, const NodePartInit* init)
      : Part(root), node_(node) {}
  NodePart(const NodePart&) = delete;
  ~NodePart() override = default;

  void Trace(Visitor* visitor) const override;
  Node* RelevantNode() const override;
  String ToString() const override;

  // NodePart API
  Node* node() const { return node_; }

 protected:
  bool IsValid() override;
  Document* GetDocument() const override;

 private:
  Member<Node> node_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_PART_H_
