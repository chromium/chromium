// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/initiator_navigation_state_impl.h"

#include <utility>

#include "content/browser/browser_context_impl.h"
#include "content/browser/site_instance_impl.h"

namespace content {

InitiatorNavigationStateImpl::InitiatorNavigationStateImpl(
    const blink::InitiatorStateToken& initiator_state_token,
    const blink::DocumentToken& document_token,
    const blink::LocalFrameToken& token,
    ChildProcessId process_id,
    const PolicyContainerHost* policy_container_host,
    scoped_refptr<SiteInstanceImpl> site_instance)
    : initiator_state_token_(initiator_state_token),
      document_token_(document_token),
      frame_token_(token),
      process_id_(process_id),
      policy_container_policies_(policy_container_host->policies().Clone()),
      site_instance_(std::move(site_instance)) {
  CHECK(site_instance_);
}

InitiatorNavigationStateImpl::~InitiatorNavigationStateImpl() {
  if (browser_context_) {
    browser_context_->RemoveInitiatorNavigationStateFromMap(this);
  }
}

}  // namespace content
