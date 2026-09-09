// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_LIFECYCLE_STATE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_LIFECYCLE_STATE_IMPL_H_

#include <iosfwd>

#include "content/common/content_export.h"

namespace content {

// Defines different states the RenderFrameHost can be in during its lifetime
// i.e., from point of creation to deletion. See
// `RenderFrameHostImpl::SetLifecycleState`.
// NOTE: this must be kept consistent with the
// RenderFrameHostImpl.LifecycleState enum in chrome_track_event.proto for
// tracing.
enum class RenderFrameHostLifecycleStateImpl {
  // This state corresponds to when a speculative RenderFrameHost is created
  // for an ongoing navigation (to new URL) but the navigation hasn't reached
  // ReadyToCommitNavigation stage yet, mainly created for performance
  // optimization. The frame can only be created in this state and no
  // transitions happen to this state.
  //
  // Transitions from this state happen to one of:
  // - kPendingCommit - when cross-RenderFrameHost navigation commits in
  // the renderer and becomes ready to commit and the //content embedders are
  // notified about the navigation's association with this RenderFrameHost.
  // - kActive -- when speculative RenderFrameHost is swapped indirectly
  // instead of following the full navigation path (known as "early commit").
  // This happens when current RenderFrameHost is not live. The work to
  // remove this transition is tracked in crbug.com/1072817.
  //
  // Speculative RenderFrameHost deletion happens without running any unload
  // handlers and with RenderFrameHostLifecycleStateImpl remaining in
  // kSpeculative state.
  //
  // Note that the term speculative is used, because the navigation might be
  // canceled or redirected and the RenderFrameHost might get deleted before
  // being used.
  kSpeculative,

  // This state corresponds to when a cross-RenderFrameHost navigation is
  // waiting for an acknowledgment from the renderer to swap the
  // RenderFrameHost.
  //
  // Note that cross-document same-RenderFrameHost navigations are not covered
  // by this state, despite going through ReadyToCommitNavigation (the
  // RenderFrameHost will be considered current and be in either kActive or
  // kPrerendering state). The work to eliminate cross-document
  // same-RenderFrameHost navigations is tracked in crbug.com/936696.
  //
  // Transitions from this state happen to one of:
  // - kActive -- when a cross-RenderFrameHost navigation commits inside
  // the primary frame tree.
  // - kPrerendering -- when a cross-RenderFrameHost navigation commits
  // inside prerendered frame tree.
  // - kReadyToBeDeleted -- when the navigation gets aborted. The work to
  // eliminate this is tracked in crbug.com/999255.
  //
  // Transition to this state only happens from kSpeculative state when a
  // speculative RenderFrameHost created for cross-RenderFrameHost navigation
  // commits in the renderer.
  kPendingCommit,

  // Prerender2:
  // This state corresponds to when a RenderFrameHost is the current one in
  // its RenderFrameHostManager and FrameTreeNode for a prerendered frame
  // tree. Documents in this state are invisible to the user and aren't
  // allowed to show any UI changes, but the page is allowed to load and run
  // in the background. Documents in kPrerendering state can be evicted
  // (cancelling prerendering) at any time.
  //
  // A prerendered page is created by an initial navigation in a prerendered
  // frame tree. For the prerendered page to be shown to the user, another
  // navigation in the primary frame tree activates the prerendered page.
  //
  // Transitions from this state happen to one of:
  // - kActive -- when the prerendered page is activated.
  // - kRunningUnloadHandlers -- when a navigation commits in a prerendered
  // frame tree, unloading the previous one.
  // - kReadyToBeDeleted -- when prerendering is cancelled and the prerendered
  // page is deleted.
  //
  // Document can be created in kPrerendering state (while initializing root
  // and child in a prerendered frame tree).
  //
  // Transition to kPrerendering can happen from kPendingCommit (when
  // cross-RenderFrameHost navigation commits inside a prerendered frame
  // tree).
  //
  // Please note that Prerender2 is an experimental feature behind the flag.
  //
  // Note that at the moment, this state is *not* used for RenderFrameHosts in
  // nested FrameTrees inside prerendered pages. See crbug.com/1232528,
  // crbug.com/1244274 for more discussion on whether or not we should support
  // nested FrameTrees inside prerendered pages.
  kPrerendering,

  // This state corresponds to when a RenderFrameHost is the current one in
  // its RenderFrameHostManager/FrameTreeNode inside a primary FrameTree or
  // its descendant FrameTrees. In this state, RenderFrameHost is visible to
  // the user. TODO(crbug.com/1232528, crbug.com/1244274): At the moment,
  // prerendered pages implicitly support nested frame trees, whose
  // RenderFrameHost's are always kActive even though they are not shown to
  // the user. We need to formally determine if prerenders should support
  // nested FrameTrees.
  //
  // Transition to kActive state may happen from one of:
  // - kSpeculative -- when a speculative RenderFrameHost commits to make it
  // the current one in primary frame tree before the corresponding navigation
  // commits.
  // - kPendingCommit -- when a cross-RenderFrameHost navigation commits. The
  // work to eliminate these early commits is tracked in crbug.com/936696
  // - kInBackForwardCache -- when restoring from BackForwardCache.
  // - kPrerendering -- when a prerendered page activates.
  //
  // RenderFrameHost can also be created in this state for an empty document
  // in a FrameTreeNode (e.g initializing root and child in an empty
  // primary FrameTree).
  //
  // Note that this state is also used for nested pages e.g., the
  // RenderFrameHosts in <fencedframe> elements, as these nested contexts do
  // not get their own lifecycle state. A RenderFrameHost can tell if it is in
  // a <fencedframe> however, by checking its `FrameTree`'s type.
  kActive,

  // This state corresponds to when RenderFrameHost is stored in
  // BackForwardCache. This happens when the user navigates away from a
  // document, so that the RenderFrameHost can be reused after a history
  // navigation. Transition to this state happens only from kActive state.
  // BackForwardCache is disabled in prerendering frame trees because a
  // prerendered page is invisible, and the user can't perform any
  // back/forward navigations.
  kInBackForwardCache,

  // This state corresponds to when RenderFrameHost has started running unload
  // handlers (this includes handlers for the "unload", "pagehide", and
  // "visibilitychange" events). An event such as navigation commit or
  // detaching the frame causes the RenderFrameHost to transition to this
  // state. Then, the RenderFrameHost sends IPCs to the renderer process to
  // execute unload handlers and deletes the RenderFrame. The RenderFrameHost
  // waits for an ACK from the renderer process, either
  // mojo::AgentSchedulingGroupHost::DidUnloadRenderFrame for a navigating
  // frame or FrameHostMsg_Detach for its subframes, after which the
  // RenderFrameHost transitions to kReadyToBeDeleted state.
  //
  // Transition to this state happens only from kActive and kPrerendering
  // states. Note that eviction from BackForwardCache does not wait for unload
  // handlers, and kInBackForwardCache moves to kReadyToBeDeleted.
  kRunningUnloadHandlers,

  // This state corresponds to when RenderFrameHost has completed running the
  // unload handlers. Once all the descendant frames in other processes are
  // gone, this RenderFrameHost will delete itself. Transition to this state
  // may happen from one of kPrerendering, kActive, kInBackForwardCache or
  // kRunningUnloadHandlers states.
  kReadyToBeDeleted,
};

// Returns the string corresponding to RenderFrameHostLifecycleStateImpl, used
// for logging crash keys.
CONTENT_EXPORT const char* RenderFrameHostLifecycleStateImplToString(
    RenderFrameHostLifecycleStateImpl state);

// Used when DCHECK_STATE_TRANSITION triggers.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& o,
    const RenderFrameHostLifecycleStateImpl& s);

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_FRAME_HOST_LIFECYCLE_STATE_IMPL_H_
