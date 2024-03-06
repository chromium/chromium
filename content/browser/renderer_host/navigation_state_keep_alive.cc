// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_state_keep_alive.h"

#include <utility>

#include "content/browser/renderer_host/policy_container_host.h"

namespace content {

NavigationStateKeepAlive::NavigationStateKeepAlive(
    const blink::LocalFrameToken& token,
    scoped_refptr<PolicyContainerHost> policy_container_host)
    : frame_token_(token),
      policy_container_host_(std::move(policy_container_host)) {}

NavigationStateKeepAlive::~NavigationStateKeepAlive() = default;

}  // namespace content
