// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/frame_replication_state.h"

#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/web/web_tree_scope_type.h"

namespace content {

FrameReplicationState::FrameReplicationState()
    : active_sandbox_flags(blink::WebSandboxFlags::kNone),
      scope(blink::WebTreeScopeType::kDocument),
      insecure_request_policy(blink::kLeaveInsecureRequestsAlone),
      has_potentially_trustworthy_unique_origin(false),
      has_received_user_gesture(false),
      has_received_user_gesture_before_nav(false) {}

FrameReplicationState::FrameReplicationState(
    blink::WebTreeScopeType scope,
    const std::string& name,
    const std::string& unique_name,
    blink::WebInsecureRequestPolicy insecure_request_policy,
    const std::vector<uint32_t>& insecure_navigations_set,
    bool has_potentially_trustworthy_unique_origin,
    bool has_received_user_gesture,
    bool has_received_user_gesture_before_nav,
    blink::FrameOwnerElementType owner_type)
    : origin(),
      name(name),
      unique_name(unique_name),
      active_sandbox_flags(blink::WebSandboxFlags::kNone),
      scope(scope),
      insecure_request_policy(insecure_request_policy),
      insecure_navigations_set(insecure_navigations_set),
      has_potentially_trustworthy_unique_origin(
          has_potentially_trustworthy_unique_origin),
      has_received_user_gesture(has_received_user_gesture),
      has_received_user_gesture_before_nav(
          has_received_user_gesture_before_nav),
      frame_owner_element_type(owner_type) {}

FrameReplicationState::FrameReplicationState(
    const FrameReplicationState& other) = default;

FrameReplicationState::~FrameReplicationState() {
}

}  // namespace content
