// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_inputs_root.h"

#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

void CompositingInputsRoot::Update(PaintLayer* new_root_layer) {
  DCHECK(new_root_layer);

  if (!root_layer_) {
    // This is the first time we call Update() so just let set the root layer.
    root_layer_ = new_root_layer;
    return;
  }

  if (root_layer_ == new_root_layer)
    return;

  PaintLayer* common_ancestor =
      const_cast<PaintLayer*>(root_layer_->CommonAncestor(new_root_layer));
  if (!common_ancestor)
    common_ancestor = const_cast<PaintLayer*>(root_layer_->Root());

  root_layer_->SetChildNeedsCompositingInputsUpdateUpToAncestor(
      common_ancestor);
  new_root_layer->SetChildNeedsCompositingInputsUpdateUpToAncestor(
      common_ancestor);

  root_layer_ = common_ancestor;
}

}  // namespace blink
