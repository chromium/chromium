// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_PART_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_PART_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_node_part_init.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class PartRoot;

// Implementation of the NodePart class, which is part of the DOM Parts API.
// A NodePart stores a reference to a single |Node| in the DOM tree.
class CORE_EXPORT NodePart : public Part {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NodePart* Create(Node* node, const NodePartInit* init) {
    return MakeGarbageCollected<NodePart>(node, init);
  }
  // TODO(crbug.com/1453291): Handle the init parameter.
  NodePart(Node* node, const NodePartInit* init) : node_(node) {}
  NodePart(const NodePart&) = delete;
  ~NodePart() override = default;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(node_);
    Part::Trace(visitor);
  }

  // NodePart API
  Node* node() const { return node_; }
  // TODO(crbug.com/1453291) Implement this method.
  PartRoot* root() const override { return nullptr; }

 private:
  Member<Node> node_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NODE_PART_H_
