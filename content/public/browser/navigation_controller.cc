// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_controller.h"

#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/was_activated_option.h"

namespace content {

NavigationController::LoadURLParams::LoadURLParams(const GURL& url)
    : url(url),
      load_type(LOAD_TYPE_DEFAULT),
      transition_type(ui::PAGE_TRANSITION_LINK),
      frame_tree_node_id(RenderFrameHost::kNoFrameTreeNodeId),
      is_renderer_initiated(false),
      override_user_agent(UA_OVERRIDE_INHERIT),
      post_data(nullptr),
      can_load_local_resources(false),
      should_replace_current_entry(false),
      has_user_gesture(false),
      should_clear_history_list(false),
      started_from_context_menu(false),
      navigation_ui_data(nullptr),
      was_activated(WasActivatedOption::kUnknown) {}

NavigationController::LoadURLParams::~LoadURLParams() {
}

}  // namespace content
