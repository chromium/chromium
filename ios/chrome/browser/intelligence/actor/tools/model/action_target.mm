// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"

#import "base/check.h"

namespace actor {

ActionTarget::ActionTarget() = default;
ActionTarget::~ActionTarget() = default;
ActionTarget::ActionTarget(const ActionTarget&) = default;
ActionTarget& ActionTarget::operator=(const ActionTarget&) = default;
ActionTarget::ActionTarget(ActionTarget&&) = default;
ActionTarget& ActionTarget::operator=(ActionTarget&&) = default;

// static
ActionTarget ActionTarget::FromProto(
    const optimization_guide::proto::ActionTarget& target) {
  ActionTarget local_target;
  if (target.has_coordinate()) {
    CoordinateTarget coord;
    coord.x = target.coordinate().x();
    coord.y = target.coordinate().y();
    coord.pixel_type = target.coordinate().pixel_type();
    local_target.coordinate_ = coord;
  } else if (target.has_content_node_id() && target.has_document_identifier()) {
    NodeIdTarget node;
    node.content_node_id = target.content_node_id();
    node.document_identifier = target.document_identifier().serialized_token();
    local_target.node_id_ = node;
  }
  return local_target;
}

void ActionTarget::UpdateCoordinate(int x, int y) {
  CHECK(coordinate_);
  coordinate_->x = x;
  coordinate_->y = y;
}

}  // namespace actor
