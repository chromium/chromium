// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/action_target.h"

#import <utility>

#import "base/check.h"
#import "base/values.h"

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

base::DictValue ActionTarget::ToDictValue() const {
  base::DictValue dict;
  if (coordinate_.has_value()) {
    base::DictValue coord_dict;
    coord_dict.Set("x", coordinate_->x);
    coord_dict.Set("y", coordinate_->y);
    coord_dict.Set("pixelType", static_cast<int>(coordinate_->pixel_type));
    dict.Set("coordinate", std::move(coord_dict));
  } else if (node_id_.has_value()) {
    dict.Set("contentNodeId", static_cast<int>(node_id_->content_node_id));
  }
  return dict;
}

}  // namespace actor
