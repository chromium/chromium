// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/attribute_part.h"

#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/part_root.h"

namespace blink {

// static
AttributePart* AttributePart::Create(PartRootUnion* root_union,
                                     Node* node,
                                     String local_name,
                                     bool automatic,
                                     const PartInit* init) {
  return MakeGarbageCollected<AttributePart>(
      *PartRoot::GetPartRootFromUnion(root_union), *node, local_name, automatic,
      init);
}

AttributePart::AttributePart(PartRoot& root,
                             Node& node,
                             String local_name,
                             bool automatic,
                             const Vector<String> metadata)
    : NodePart(root, node, !automatic, metadata),
      local_name_(local_name),
      automatic_(automatic) {}

Part* AttributePart::ClonePart(NodeCloningData& data, Node& node_clone) const {
  DCHECK(IsValid());
  return MakeGarbageCollected<AttributePart>(
      data.CurrentPartRoot(), node_clone, local_name_, automatic_, metadata());
}

}  // namespace blink
