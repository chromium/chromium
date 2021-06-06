// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"

#include <memory>

namespace blink {

const PropertyTreeState& PropertyTreeState::Root() {
  DEFINE_STATIC_LOCAL(
      const PropertyTreeState, root,
      (TransformPaintPropertyNode::Root(), ClipPaintPropertyNode::Root(),
       EffectPaintPropertyNode::Root()));
  return root;
}

String PropertyTreeStateOrAlias::ToString() const {
  return String::Format("t:%p c:%p e:%p", transform_, clip_, effect_);
}

#if DCHECK_IS_ON()

String PropertyTreeStateOrAlias::ToTreeString() const {
  return "transform:\n" + Transform().ToTreeString() + "\nclip:\n" +
         Clip().ToTreeString() + "\neffect:\n" + Effect().ToTreeString();
}

#endif

std::unique_ptr<JSONObject> PropertyTreeStateOrAlias::ToJSON() const {
  std::unique_ptr<JSONObject> result = std::make_unique<JSONObject>();
  result->SetObject("transform", transform_->ToJSON());
  result->SetObject("clip", clip_->ToJSON());
  result->SetObject("effect", effect_->ToJSON());
  return result;
}

std::ostream& operator<<(std::ostream& os,
                         const PropertyTreeStateOrAlias& state) {
  return os << state.ToString().Utf8();
}

}  // namespace blink
