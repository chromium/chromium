// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTION_TARGET_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTION_TARGET_H_

#import <optional>
#import <string>

#import "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

// Represents a local representation of an ActionTarget proto.
class ActionTarget {
 public:
  // Target specified by (x, y) coordinates.
  struct CoordinateTarget {
    int x;
    int y;
    optimization_guide::proto::Coordinate_PixelType pixel_type;

    bool operator==(const CoordinateTarget& other) const = default;
  };

  // Target specified by document identifier and content node ID.
  struct NodeIdTarget {
    int32_t content_node_id;
    std::string document_identifier;

    bool operator==(const NodeIdTarget& other) const = default;
  };

  ActionTarget();
  ~ActionTarget();
  ActionTarget(const ActionTarget&);
  ActionTarget& operator=(const ActionTarget&);
  ActionTarget(ActionTarget&&);
  ActionTarget& operator=(ActionTarget&&);

  // Converts a proto target to the C++ representation.
  // Follows the implementation of `ToPageTarget` in
  // chrome/browser/actor/actor_proto_conversion.cc; in cases where both
  // coordinate and node are provided, coordinate takes precedence.
  static ActionTarget FromProto(
      const optimization_guide::proto::ActionTarget& target);

  // Returns the coordinate target if present.
  const std::optional<CoordinateTarget>& coordinate() const {
    return coordinate_;
  }

  // Returns the node ID target if present.
  const std::optional<NodeIdTarget>& node_id() const { return node_id_; }

  // Validates the target representation. Returns true if the target is valid.
  bool is_valid() const {
    return coordinate_.has_value() || node_id_.has_value();
  }

  // Updates the coordinate values. Expects `coordinate_` to be set.
  void UpdateCoordinate(int x, int y);

  bool operator==(const ActionTarget& other) const = default;

 private:
  std::optional<CoordinateTarget> coordinate_;
  std::optional<NodeIdTarget> node_id_;
};

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_MODEL_ACTION_TARGET_H_
