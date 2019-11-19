// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/static_selection.h"

#include "third_party/blink/renderer/core/editing/selection_template.h"

namespace blink {

// static
StaticSelection* StaticSelection::FromSelectionInDOMTree(
    const SelectionInDOMTree& selection) {
  return MakeGarbageCollected<StaticSelection>(selection);
}

// static
StaticSelection* StaticSelection::FromSelectionInFlatTree(
    const SelectionInFlatTree& seleciton) {
  return MakeGarbageCollected<StaticSelection>(seleciton);
}

StaticSelection::StaticSelection(const SelectionInDOMTree& selection)
    : anchor_node_(selection.Base().ComputeContainerNode()),
      anchor_offset_(selection.Base().ComputeOffsetInContainerNode()),
      focus_node_(selection.Extent().ComputeContainerNode()),
      focus_offset_(selection.Extent().ComputeOffsetInContainerNode()) {}

StaticSelection::StaticSelection(const SelectionInFlatTree& seleciton)
    : anchor_node_(seleciton.Base().ComputeContainerNode()),
      anchor_offset_(seleciton.Base().ComputeOffsetInContainerNode()),
      focus_node_(seleciton.Extent().ComputeContainerNode()),
      focus_offset_(seleciton.Extent().ComputeOffsetInContainerNode()) {}

bool StaticSelection::isCollapsed() const {
  return anchor_node_ == focus_node_ && anchor_offset_ == focus_offset_;
}

void StaticSelection::Trace(blink::Visitor* visitor) {
  visitor->Trace(anchor_node_);
  visitor->Trace(focus_node_);
  ScriptWrappable::Trace(visitor);
}

}  //  namespace blink
