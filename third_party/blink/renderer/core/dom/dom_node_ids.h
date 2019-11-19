// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOM_NODE_IDS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOM_NODE_IDS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

DECLARE_WEAK_IDENTIFIER_MAP(Node, DOMNodeId);

class CORE_EXPORT DOMNodeIds {
  STATIC_ONLY(DOMNodeIds);

 public:
  static DOMNodeId ExistingIdForNode(Node*);
  static DOMNodeId IdForNode(Node*);
  static Node* NodeForId(DOMNodeId);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOM_NODE_IDS_H_
