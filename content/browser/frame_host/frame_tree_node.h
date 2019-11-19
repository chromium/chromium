// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/frame_host/frame_tree_node_blame_context.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_frame_host_manager.h"
#include "content/common/content_export.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/frame_replication_state.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/frame/user_activation_state.h"
#include "third_party/blink/public/common/frame/user_activation_update_type.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class FrameTree;
class NavigationRequest;
class Navigator;
class RenderFrameHostImpl;
class NavigationEntryImpl;
struct ContentSecurityPolicyHeader;

// When a page contains iframes, its renderer process maintains a tree structure
// of those frames. We are mirroring this tree in the browser process. This
// class represents a node in this tree and is a wrapper for all objects that
// are frame-specific (as opposed to page-specific).
//
// Each FrameTreeNode has a current RenderFrameHost, which can change over
// time as the frame is navigated. Any immediate subframes of the current
// document are tracked using FrameTreeNodes owned by the current
// RenderFrameHost, rather than as children of FrameTreeNode itself. This
// allows subframe FrameTreeNodes to stay alive while a RenderFrameHost is
// still alive - for example while pending deletion, after a new current
// RenderFrameHost has replaced it.
class CONTENT_EXPORT FrameTreeNode {
 public:
  class Observer {
   public:
    // Invoked when a FrameTreeNode is being destroyed.
    virtual void OnFrameTreeNodeDestroyed(FrameTreeNode* node) {}

    // Invoked when a FrameTreeNode becomes focused.
    virtual void OnFrameTreeNodeFocused(FrameTreeNode* node) {}

    virtual ~Observer() {}
  };

  static const int kFrameTreeNodeInvalidId;

  // Returns the FrameTreeNode with the given global |frame_tree_node_id|,
  // regardless of which FrameTree it is in.
  static FrameTreeNode* GloballyFindByID(int frame_tree_node_id);

  // Callers are are expected to initialize sandbox flags separately after
  // calling the constructor.
  FrameTreeNode(FrameTree* frame_tree,
                Navigator* navigator,
                FrameTreeNode* parent,
                blink::WebTreeScopeType scope,
                const std::string& name,
                const std::string& unique_name,
                bool is_created_by_script,
                const base::UnguessableToken& devtools_frame_token,
                const FrameOwnerProperties& frame_owner_properties,
                blink::FrameOwnerElementType owner_type);

  ~FrameTreeNode();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsMainFrame() const;

  // Clears any state in this node which was set by the document itself (CSP
  // Headers, Feature Policy Headers, and CSP-set sandbox flags), and notifies
  // proxies as appropriate. Invoked after committing navigation to a new
  // document (since the new document comes with a fresh set of CSP and
  // Feature-Policy HTTP headers).
  void ResetForNavigation();

  FrameTree* frame_tree() const { return frame_tree_; }
  Navigator* navigator() { return navigator_.get(); }
  RenderFrameHostManager* render_manager() { return &render_manager_; }
  int frame_tree_node_id() const { return frame_tree_node_id_; }
  const std::string& frame_name() const { return replication_state_.name; }

  const std::string& unique_name() const {
    return replication_state_.unique_name;
  }

  // See comment on the member declaration.
  const base::UnguessableToken& devtools_frame_token() const {
    return devtools_frame_token_;
  }

  size_t child_count() const { return current_frame_host()->child_count(); }

  unsigned int depth() const { return depth_; }

  FrameTreeNode* parent() const { return parent_; }

  FrameTreeNode* opener() const { return opener_; }

  FrameTreeNode* original_opener() const { return original_opener_; }

  // Gets the total number of descendants to this FrameTreeNode in addition to
  // this node.
  size_t GetFrameTreeSize() const;

  // Assigns a new opener for this node and, if |opener| is non-null, registers
  // an observer that will clear this node's opener if |opener| is ever
  // destroyed.
  void SetOpener(FrameTreeNode* opener);

  // Assigns the initial opener for this node, and if |opener| is non-null,
  // registers an observer that will clear this node's opener if |opener| is
  // ever destroyed. The value set here is the root of the tree.
  //
  // It is not possible to change the opener once it was set.
  void SetOriginalOpener(FrameTreeNode* opener);

  FrameTreeNode* child_at(size_t index) const {
    return current_frame_host()->child_at(index);
  }

  // Returns the URL of the last committed page in the current frame.
  const GURL& current_url() const {
    return current_frame_host()->GetLastCommittedURL();
  }

  // Sets the last committed URL for this frame and updates
  // has_committed_real_load accordingly.
  void SetCurrentURL(const GURL& url);

  // Returns true iff SetCurrentURL has been called with a non-blank URL.
  bool has_committed_real_load() const { return has_committed_real_load_; }

  // Returns whether the frame's owner element in the parent document is
  // collapsed, that is, removed from the layout as if it did not exist, as per
  // request by the embedder (of the content/ layer).
  bool is_collapsed() const { return is_collapsed_; }

  // Sets whether to collapse the frame's owner element in the parent document,
  // that is, to remove it from the layout as if it did not exist, as per
  // request by the embedder (of the content/ layer). Cannot be called for main
  // frames.
  //
  // This only has an effect for <iframe> owner elements, and is a no-op when
  // called on sub-frames hosted in <frame>, <object>, and <embed> elements.
  void SetCollapsed(bool collapsed);

  // Returns the origin of the last committed page in this frame.
  // WARNING: To get the last committed origin for a particular
  // RenderFrameHost, use RenderFrameHost::GetLastCommittedOrigin() instead,
  // which will behave correctly even when the RenderFrameHost is not the
  // current one for this frame (such as when it's pending deletion).
  const url::Origin& current_origin() const {
    return replication_state_.origin;
  }

  // Set the current origin and notify proxies about the update.
  void SetCurrentOrigin(const url::Origin& origin,
                        bool is_potentially_trustworthy_unique_origin);

  // Set the current name and notify proxies about the update.
  void SetFrameName(const std::string& name, const std::string& unique_name);

  // Add CSP headers to replication state, notify proxies about the update.
  void AddContentSecurityPolicies(
      const std::vector<ContentSecurityPolicyHeader>& headers);

  // Sets the current insecure request policy, and notifies proxies about the
  // update.
  void SetInsecureRequestPolicy(blink::WebInsecureRequestPolicy policy);

  // Sets the current set of insecure urls to upgrade, and notifies proxies
  // about the update.
  void SetInsecureNavigationsSet(
      const std::vector<uint32_t>& insecure_navigations_set);

  // Returns the latest frame policy (sandbox flags and container policy) for
  // this frame. This includes flags inherited from parent frames and the latest
  // flags from the <iframe> element hosting this frame. The returned policies
  // may not yet have taken effect, since "sandbox" and "allow" attribute
  // updates in an <iframe> element take effect on next navigation. To retrieve
  // the currently active policy for this frame, use effective_frame_policy().
  const blink::FramePolicy& pending_frame_policy() const {
    return pending_frame_policy_;
  }

  // Update this frame's sandbox flags and container policy.  This is called
  // when a parent frame updates the "sandbox" attribute in the <iframe> element
  // for this frame, or any of the attributes which affect the container policy
  // ("allowfullscreen", "allowpaymentrequest", "allow", and "src".)
  // These policies won't take effect until next navigation.  If this frame's
  // parent is itself sandboxed, the parent's sandbox flags are combined with
  // those in |frame_policy|.
  // Attempting to change the container policy on the main frame will have no
  // effect.
  void SetPendingFramePolicy(blink::FramePolicy frame_policy);

  // Returns the currently active frame policy for this frame, including the
  // sandbox flags which were present at the time the document was loaded, and
  // the feature policy container policy, which is set by the iframe's
  // allowfullscreen, allowpaymentrequest, and allow attributes, along with the
  // origin of the iframe's src attribute (which may be different from the URL
  // of the document currently loaded into the frame). This does not include
  // policy changes that have been made by updating the containing iframe
  // element attributes since the frame was last navigated; use
  // pending_frame_policy() for those.
  const blink::FramePolicy& effective_frame_policy() const {
    return replication_state_.frame_policy;
  }

  // Set any pending sandbox flags and container policy as active, and return
  // true if either was changed.
  bool CommitPendingFramePolicy();

  const FrameOwnerProperties& frame_owner_properties() {
    return frame_owner_properties_;
  }

  void set_frame_owner_properties(
      const FrameOwnerProperties& frame_owner_properties) {
    frame_owner_properties_ = frame_owner_properties;
  }

  bool HasSameOrigin(const FrameTreeNode& node) const {
    return replication_state_.origin.IsSameOriginWith(
        node.replication_state_.origin);
  }

  const FrameReplicationState& current_replication_state() const {
    return replication_state_;
  }

  RenderFrameHostImpl* current_frame_host() const {
    return render_manager_.current_frame_host();
  }

  // Return the node immediately preceding this node in its parent's
  // |children_|, or nullptr if there is no such node.
  FrameTreeNode* PreviousSibling() const;

  // Return the node immediately following this node in its parent's
  // |children_|, or nullptr if there is no such node.
  FrameTreeNode* NextSibling() const;

  // Returns true if this node is in a loading state.
  bool IsLoading() const;

  NavigationRequest* navigation_request() { return navigation_request_.get(); }

  // Transfers the ownership of the NavigationRequest to |render_frame_host|.
  // From ReadyToCommit to DidCommit, the NavigationRequest is owned by the
  // RenderFrameHost that is committing the navigation.
  void TransferNavigationRequestOwnership(
      RenderFrameHostImpl* render_frame_host);

  // Takes ownership of |navigation_request| and makes it the current
  // NavigationRequest of this frame. This corresponds to the start of a new
  // navigation. If there was an ongoing navigation request before calling this
  // function, it is canceled. |navigation_request| should not be null.
  void CreatedNavigationRequest(
      std::unique_ptr<NavigationRequest> navigation_request);

  // Resets the current navigation request. If |keep_state| is true, any state
  // created by the NavigationRequest (e.g. speculative RenderFrameHost,
  // loading state) will not be reset by the function.
  void ResetNavigationRequest(bool keep_state);

  // A RenderFrameHost in this node started loading.
  // |to_different_document| will be true unless the load is a fragment
  // navigation, or triggered by history.pushState/replaceState.
  // |was_previously_loading| is false if the FrameTree was not loading before.
  // The caller is required to provide this boolean as the delegate should only
  // be notified if the FrameTree went from non-loading to loading state.
  // However, when it is called, the FrameTree should be in a loading state.
  void DidStartLoading(bool to_different_document, bool was_previously_loading);

  // A RenderFrameHost in this node stopped loading.
  void DidStopLoading();

  // The load progress for a RenderFrameHost in this node was updated to
  // |load_progress|. This will notify the FrameTree which will in turn notify
  // the WebContents.
  void DidChangeLoadProgress(double load_progress);

  // Called when the user directed the page to stop loading. Stops all loads
  // happening in the FrameTreeNode. This method should be used with
  // FrameTree::ForEach to stop all loads in the entire FrameTree.
  bool StopLoading();

  // Returns the time this frame was last focused.
  base::TimeTicks last_focus_time() const { return last_focus_time_; }

  // Called when this node becomes focused.  Updates the node's last focused
  // time and notifies observers.
  void DidFocus();

  // Called when the user closed the modal dialogue for BeforeUnload and
  // cancelled the navigation. This should stop any load happening in the
  // FrameTreeNode.
  void BeforeUnloadCanceled();

  // Returns the BlameContext associated with this node.
  FrameTreeNodeBlameContext& blame_context() { return blame_context_; }

  // Updates the user activation state in the browser frame tree and in the
  // frame trees in all renderer processes except the renderer for this node
  // (which initiated the update).  Returns |false| if the update tries to
  // consume an already consumed/expired transient state, |true| otherwise.  See
  // the comment on user_activation_state_ below.
  bool UpdateUserActivationState(blink::UserActivationUpdateType update_type);

  void OnSetHasReceivedUserGestureBeforeNavigation(bool value);

  // Returns the sandbox flags currently in effect for this frame. This includes
  // flags inherited from parent frames, the currently active flags from the
  // <iframe> element hosting this frame, as well as any flags set from a
  // Content-Security-Policy HTTP header. This does not include flags that have
  // have been updated in an <iframe> element but have not taken effect yet; use
  // pending_frame_policy() for those. To see the flags which will take effect
  // on navigation (which does not include the CSP-set flags), use
  // effective_frame_policy().
  blink::WebSandboxFlags active_sandbox_flags() const {
    return replication_state_.active_sandbox_flags;
  }

  // Updates the active sandbox flags in this frame, in response to a
  // Content-Security-Policy header adding additional flags, in addition to
  // those given to this frame by its parent, or in response to the
  // Feature-Policy header being set. Note that on navigation, these updates
  // will be cleared, and the flags in the pending frame policy will be applied
  // to the frame.
  void UpdateFramePolicyHeaders(
      blink::WebSandboxFlags sandbox_flags,
      const blink::ParsedFeaturePolicy& parsed_header);

  // Returns whether the frame received a user gesture.
  bool has_received_user_gesture() const {
    return replication_state_.has_received_user_gesture;
  }

  // Returns whether the frame received a user gesture on a previous navigation
  // on the same eTLD+1.
  bool has_received_user_gesture_before_nav() const {
    return replication_state_.has_received_user_gesture_before_nav;
  }

  // When a tab is discarded, WebContents sets was_discarded on its
  // root FrameTreeNode.
  // In addition, when a child frame is created, this bit is passed on from
  // parent to child.
  // When a navigation request is created, was_discarded is passed on to the
  // request and reset to false in FrameTreeNode.
  void set_was_discarded() { was_discarded_ = true; }
  bool was_discarded() const { return was_discarded_; }

  // Returns the sticky bit of the User Activation v2 state of the
  // |FrameTreeNode|.
  bool HasBeenActivated() const {
    return user_activation_state_.HasBeenActive();
  }

  // Returns the transient bit of the User Activation v2 state of the
  // |FrameTreeNode|.
  bool HasTransientUserActivation() {
    return user_activation_state_.IsActive();
  }

  // Transfers user activation state from |source| frame to |this| and notifies
  // proxies in non-source and non-target renderer processes to transfer the
  // activation state from the source proxy to the target.
  void TransferUserActivationFrom(RenderFrameHostImpl* source_rfh);

  // Remove history entries for all frames created by script in this frame's
  // subtree. If a frame created by a script is removed, then its history entry
  // will never be reused - this saves memory.
  void PruneChildFrameNavigationEntries(NavigationEntryImpl* entry);

  blink::FrameOwnerElementType frame_owner_element_type() const {
    return replication_state_.frame_owner_element_type;
  }
  // Only meaningful to call on a root frame. The value of |feature_state| will
  // be nontrivial if there is an opener which is restricted in some of the
  // feature policies.
  void SetOpenerFeaturePolicyState(
      const blink::FeaturePolicy::FeatureState& feature_state);

 private:
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessFeaturePolicyBrowserTest,
                           ContainerPolicyDynamic);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessFeaturePolicyBrowserTest,
                           ContainerPolicySandboxDynamic);

  class OpenerDestroyedObserver;

  FrameTreeNode* GetSibling(int relative_offset) const;

  bool NotifyUserActivation();

  bool ConsumeTransientUserActivation();

  bool ClearUserActivation();

  // Verify that the renderer process is allowed to set user activation on this
  // frame by checking whether this frame's RenderWidgetHost had previously seen
  // an input event that might lead to user activation. If user activation
  // should be allowed, this returns true and also clears corresponding pending
  // user activation state in the widget. Otherwise, this returns false.
  bool VerifyUserActivation();

  // The next available browser-global FrameTreeNode ID.
  static int next_frame_tree_node_id_;

  // The FrameTree that owns us.
  FrameTree* frame_tree_;  // not owned.

  // The Navigator object responsible for managing navigations at this node
  // of the frame tree.
  scoped_refptr<Navigator> navigator_;

  // Manages creation and swapping of RenderFrameHosts for this frame.  This
  // must be declared before |children_| so that it gets deleted after them.
  // That's currently necessary so that RenderFrameHostImpl's destructor can
  // call GetProcess.
  RenderFrameHostManager render_manager_;

  // A browser-global identifier for the frame in the page, which stays stable
  // even if the frame does a cross-process navigation.
  const int frame_tree_node_id_;

  // The parent node of this frame. |nullptr| if this node is the root.
  FrameTreeNode* const parent_;

  // Number of edges from this node to the root. 0 if this is the root.
  const unsigned int depth_;

  // The frame that opened this frame, if any.  Will be set to null if the
  // opener is closed, or if this frame disowns its opener by setting its
  // window.opener to null.
  FrameTreeNode* opener_;

  // An observer that clears this node's |opener_| if the opener is destroyed.
  // This observer is added to the |opener_|'s observer list when the |opener_|
  // is set to a non-null node, and it is removed from that list when |opener_|
  // changes or when this node is destroyed.  It is also cleared if |opener_|
  // is disowned.
  std::unique_ptr<OpenerDestroyedObserver> opener_observer_;

  // The frame that opened this frame, if any. Contrary to opener_, this
  // cannot be changed unless the original opener is destroyed.
  FrameTreeNode* original_opener_;

  // An observer that clears this node's |original_opener_| if the opener is
  // destroyed.
  std::unique_ptr<OpenerDestroyedObserver> original_opener_observer_;

  // Whether this frame has committed any real load, replacing its initial
  // about:blank page.
  bool has_committed_real_load_;

  // Whether the frame's owner element in the parent document is collapsed.
  bool is_collapsed_;

  // Track information that needs to be replicated to processes that have
  // proxies for this frame.
  FrameReplicationState replication_state_;

  // Track the pending sandbox flags and container policy for this frame. When a
  // parent frame dynamically updates 'sandbox', 'allow', 'allowfullscreen',
  // 'allowpaymentrequest' or 'src' attributes, the updated policy for the frame
  // is stored here, and transferred into replication_state_.frame_policy when
  // they take effect on the next frame navigation.
  blink::FramePolicy pending_frame_policy_;

  // Whether the frame was created by javascript.  This is useful to prune
  // history entries when the frame is removed (because frames created by
  // scripts are never recreated with the same unique name - see
  // https://crbug.com/500260).
  bool is_created_by_script_;

  // Used for devtools instrumentation and trace-ability. The token is
  // propagated to Blink's LocalFrame and both Blink and content/
  // can tag calls and requests with this token in order to attribute them
  // to the context frame.
  // |devtools_frame_token_| is only defined by the browser process and is never
  // sent back from the renderer in the control calls. It should be never used
  // to look up the FrameTreeNode instance.
  base::UnguessableToken devtools_frame_token_;

  // Tracks the scrolling and margin properties for this frame.  These
  // properties affect the child renderer but are stored on its parent's
  // frame element.  When this frame's parent dynamically updates these
  // properties, we update them here too.
  //
  // Note that dynamic updates only take effect on the next frame navigation.
  FrameOwnerProperties frame_owner_properties_;

  // Owns an ongoing NavigationRequest until it is ready to commit. It will then
  // be reset and a RenderFrameHost will be responsible for the navigation.
  std::unique_ptr<NavigationRequest> navigation_request_;

  // List of objects observing this FrameTreeNode.
  base::ObserverList<Observer>::Unchecked observers_;

  base::TimeTicks last_focus_time_;

  bool was_discarded_;

  // The user activation state of the current frame.  See |UserActivationState|
  // for details on how this state is maintained.
  blink::UserActivationState user_activation_state_;

  // A helper for tracing the snapshots of this FrameTreeNode and attributing
  // browser process activities to this node (when possible).  It is unrelated
  // to the core logic of FrameTreeNode.
  FrameTreeNodeBlameContext blame_context_;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeNode);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_H_
