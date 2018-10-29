// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/ref_counted_property_tree_state.h"

#include <memory>

namespace blink {

const RefCountedPropertyTreeState& RefCountedPropertyTreeState::Root() {
  DEFINE_STATIC_LOCAL(
      std::unique_ptr<RefCountedPropertyTreeState>, root,
      (std::make_unique<RefCountedPropertyTreeState>(
          &TransformPaintPropertyNode::Root(), &ClipPaintPropertyNode::Root(),
          &EffectPaintPropertyNode::Root())));
  return *root;
}

const CompositorElementId RefCountedPropertyTreeState::GetCompositorElementId(
    const CompositorElementIdSet& element_ids) const {
  // The effect or transform nodes could have a compositor element id. The order
  // doesn't matter as the element id should be the same on all that have a
  // non-default CompositorElementId.
  //
  // Note that RefCountedPropertyTreeState acts as a context that accumulates
  // state as we traverse the tree building layers. This means that we could see
  // a compositor element id 'A' for a parent layer in conjunction with a
  // compositor element id 'B' for a child layer. To preserve uniqueness of
  // element ids, then, we check for presence in the |element_ids| set (which
  // represents element ids already previously attached to a layer). This is an
  // interim step while we pursue broader rework of animation subsystem noted in
  // http://crbug.com/709137.
  if (Effect()->GetCompositorElementId() &&
      !element_ids.count(Effect()->GetCompositorElementId()))
    return Effect()->GetCompositorElementId();
  if (Transform()->GetCompositorElementId() &&
      !element_ids.count(Transform()->GetCompositorElementId()))
    return Transform()->GetCompositorElementId();
  return CompositorElementId();
}

}  // namespace blink
