// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INITIATOR_NAVIGATION_STATE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_INITIATOR_NAVIGATION_STATE_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/initiator_navigation_state.h"
#include "content/public/common/child_process_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

class PolicyContainerHost;
class RenderFrameHostImpl;
class SiteInstanceImpl;

// A record of the state of a document at one point in time. This is used to
// keep track of navigation initiator properties even after the initiator
// document goes away.
class CONTENT_EXPORT InitiatorNavigationStateImpl
    : public InitiatorNavigationState {
 public:
  InitiatorNavigationStateImpl(const InitiatorNavigationStateImpl&) = delete;
  InitiatorNavigationStateImpl& operator=(const InitiatorNavigationStateImpl&) =
      delete;

  blink::LocalFrameToken frame_token() const { return frame_token_; }

  // The PolicyContainerPolicies of the document at the moment the
  // InitiatorNavigationState was created.
  const PolicyContainerPolicies& policy_container_policies() const {
    return policy_container_policies_;
  }

  // The SiteInstance of the RenderFrameHost. This should never be null.
  SiteInstanceImpl* site_instance() const {
    CHECK(site_instance_);
    return site_instance_.get();
  }

 private:
  friend class RenderFrameHostImpl;

  // `policy_container_host` is the PolicyContainer of the RenderFrameHost that
  // creates this InitiatorNavigationStateImpl. It should never be null.
  InitiatorNavigationStateImpl(const blink::LocalFrameToken& token,
                               ChildProcessId process_id,
                               const PolicyContainerHost* policy_container_host,
                               scoped_refptr<SiteInstanceImpl> site_instance);

  ~InitiatorNavigationStateImpl() override;

  // The frame token for the RenderFrameHost this state is associated with.
  const blink::LocalFrameToken frame_token_;

  // The process ID of the document this state is associated with.
  const ChildProcessId process_id_;

  // A copy of the PolicyContainerPolicies of the document at the moment the
  // InitiatorNavigationState was created.
  const PolicyContainerPolicies policy_container_policies_;

  // The SiteInstance of the document that created the navigation.
  scoped_refptr<SiteInstanceImpl> site_instance_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INITIATOR_NAVIGATION_STATE_IMPL_H_
