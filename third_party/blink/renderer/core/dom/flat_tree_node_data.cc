// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/flat_tree_node_data.h"

#include "third_party/blink/renderer/core/html/html_slot_element.h"

namespace blink {

void FlatTreeNodeData::Trace(Visitor* visitor) const {
  visitor->Trace(assigned_slot_);
  visitor->Trace(previous_in_assigned_nodes_);
  visitor->Trace(next_in_assigned_nodes_);
  visitor->Trace(manually_assigned_slot_);
}

}  // namespace blink
