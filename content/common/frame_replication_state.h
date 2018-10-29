// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
#define CONTENT_COMMON_FRAME_REPLICATION_STATE_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/common/content_security_policy_header.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "url/origin.h"

namespace blink {
enum class WebTreeScopeType;
enum class WebSandboxFlags;
}

namespace content {

// This structure holds information that needs to be replicated between a
// RenderFrame and any of its associated RenderFrameProxies.
struct CONTENT_EXPORT FrameReplicationState {
  FrameReplicationState();
  FrameReplicationState(blink::WebTreeScopeType scope,
                        const std::string& name,
                        const std::string& unique_name,
                        blink::WebInsecureRequestPolicy insecure_request_policy,
                        const std::vector<uint32_t>& insecure_navigations_set,
                        bool has_potentially_trustworthy_unique_origin,
                        bool has_received_user_gesture,
                        bool has_received_user_gesture_before_nav,
                        blink::FrameOwnerElementType owner_type);
  FrameReplicationState(const FrameReplicationState& other);
  ~FrameReplicationState();

  // Current origin of the frame. This field is updated whenever a frame
  // navigation commits.
  //
  // TODO(alexmos): For now, |origin| updates are immediately sent to all frame
  // proxies when in --site-per-process mode. This isn't ideal, since Blink
  // typically needs a proxy's origin only when performing security checks on
  // the ancestors of a local frame.  So, as a future improvement, we could
  // delay sending origin updates to proxies until they have a local descendant
  // (if ever). This would reduce leaking a user's browsing history into a
  // compromized renderer.
  url::Origin origin;

  // The assigned name of the frame (see WebFrame::assignedName()).
  //
  // |name| is set when a new child frame is created using the value of the
  // <iframe> element's "name" attribute (see
  // RenderFrameHostImpl::OnCreateChildFrame), and it is updated dynamically
  // whenever a frame sets its window.name.
  //
  // |name| updates are immediately sent to all frame proxies (when in
  // --site-per-process mode), so that other frames can look up or navigate a
  // frame using its updated name (e.g., using window.open(url, frame_name)).
  std::string name;

  // Unique name of the frame (see WebFrame::uniqueName()).
  //
  // |unique_name| is used in heuristics that try to identify the same frame
  // across different, unrelated navigations (i.e. to refer to the frame
  // when going back/forward in session history OR when refering to the frame
  // in layout tests results).
  //
  // |unique_name| needs to be replicated to ensure that unique name for a given
  // frame is the same across all renderers - without replication a renderer
  // might arrive at a different value when recalculating the unique name from
  // scratch.
  std::string unique_name;

  // Parsed feature policy header. May be empty if no header was sent with the
  // document.
  blink::ParsedFeaturePolicy feature_policy_header;

  // Contains the currently active sandbox flags for this frame, including flags
  // inherited from parent frames, the currently active flags from the <iframe>
  // element hosting this frame, as well as any flags set from a
  // Content-Security-Policy HTTP header.
  blink::WebSandboxFlags active_sandbox_flags;

  // Iframe sandbox flags and container policy currently in effect for the
  // frame. Container policy may be empty if this is the top-level frame.
  // |sandbox_flags| are initialized for new child frames using the value of the
  // <iframe> element's "sandbox" attribute, combined with any sandbox flags in
  // effect for the parent frame. This does *not* include any flags set by a
  // Content-Security-Policy header delivered with the framed document.
  //
  // When a parent frame updates an <iframe>'s sandbox attribute via
  // JavaScript, |sandbox_flags| are updated only after the child frame commits
  // a navigation that makes the updated flags take effect.  This is also the
  // point at which updates are sent to proxies (see
  // CommitPendingFramePolicy()). The proxies need updated flags so that they
  // can be inherited properly if a proxy ever becomes a parent of a local
  // frame.
  blink::FramePolicy frame_policy;

  // Accumulated CSP headers - gathered from http headers, <meta> elements,
  // parent frames (in case of about:blank frames).
  std::vector<ContentSecurityPolicyHeader> accumulated_csp_headers;

  // Whether the frame is in a document tree or a shadow tree, per the Shadow
  // DOM spec: https://w3c.github.io/webcomponents/spec/shadow/
  // Note: This should really be const, as it can never change once a frame is
  // created. However, making it const makes it a pain to embed into IPC message
  // params: having a const member implicitly deletes the copy assignment
  // operator.
  blink::WebTreeScopeType scope;

  // The insecure request policy that a frame's current document is enforcing.
  // Updates are immediately sent to all frame proxies when frames live in
  // different processes.
  blink::WebInsecureRequestPolicy insecure_request_policy;

  // The upgrade insecure navigations set that a frame's current document is
  // enforcing. Updates are immediately sent to all frame proxies when frames
  // live in different processes. Elements in the set are hashes of hosts to be
  // upgraded.
  std::vector<uint32_t> insecure_navigations_set;

  // True if a frame's origin is unique and should be considered potentially
  // trustworthy.
  bool has_potentially_trustworthy_unique_origin;

  // Whether the frame has ever received a user gesture anywhere.
  bool has_received_user_gesture;

  // Whether the frame has received a user gesture in a previous navigation so
  // long as a the frame has staying on the same eTLD+1.
  bool has_received_user_gesture_before_nav;

  // The type of the (local) frame owner for this frame in the parent process.
  // Note: This should really be const, as it can never change once a frame is
  // created. However, making it const makes it a pain to embed into IPC message
  // params: having a const member implicitly deletes the copy assignment
  // operator.
  blink::FrameOwnerElementType frame_owner_element_type =
      blink::FrameOwnerElementType::kNone;

  // IMPORTANT NOTE: When adding a new member to this struct, don't forget to
  // also add a corresponding entry to the struct traits in frame_messages.h!
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_REPLICATION_STATE_H_
