// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_NODE_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_NODE_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_host_manager.h"
#include "content/browser/renderer_host/render_frame_host_owner.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/navigation_discard_reason.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "services/network/public/mojom/referrer_policy.mojom-forward.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/tree_scope_type.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom-forward.h"
#include "third_party/blink/public/mojom/webauthn/virtual_authenticator.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class NavigationRequest;
class NavigationEntryImpl;
class FrameTree;

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
class CONTENT_EXPORT FrameTreeNode : public RenderFrameHostOwner {
 public:
  class Observer {
   public:
    // Invoked when a FrameTreeNode is being destroyed.
    virtual void OnFrameTreeNodeDestroyed(FrameTreeNode* node) {}

    // Invoked when a FrameTreeNode becomes focused.
    virtual void OnFrameTreeNodeFocused(FrameTreeNode* node) {}

    // Invoked when a FrameTreeNode moves to a different BrowsingInstance and
    // the popups it opened should be disowned.
    virtual void OnFrameTreeNodeDisownedOpenee(FrameTreeNode* node) {}

    virtual ~Observer() = default;
  };

  // Returns the FrameTreeNode with the given global |frame_tree_node_id|,
  // regardless of which FrameTree it is in.
  static FrameTreeNode* GloballyFindByID(FrameTreeNodeId frame_tree_node_id);

  // Returns the FrameTreeNode for the given |rfh|. Same as
  // rfh->frame_tree_node(), but also supports nullptrs.
  static FrameTreeNode* From(RenderFrameHost* rfh);

  // Callers are are expected to initialize sandbox flags separately after
  // calling the constructor.
  FrameTreeNode(
      FrameTree& frame_tree,
      RenderFrameHostImpl* parent,
      blink::mojom::TreeScopeType tree_scope_type,
      bool is_created_by_script,
      const blink::mojom::FrameOwnerProperties& frame_owner_properties,
      blink::FrameOwnerElementType owner_type,
      const blink::FramePolicy& frame_owner);

  FrameTreeNode(const FrameTreeNode&) = delete;
  FrameTreeNode& operator=(const FrameTreeNode&) = delete;

  ~FrameTreeNode() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Frame trees may be nested so it can be the case that IsMainFrame() is true,
  // but is not the outermost main frame. In particular, !IsMainFrame() cannot
  // be used to check if the frame is an embedded frame -- use
  // !IsOutermostMainFrame() instead. NB: this does not escape guest views;
  // IsOutermostMainFrame will be true for the outermost main frame in an inner
  // guest view.
  bool IsMainFrame() const;
  bool IsOutermostMainFrame() const;

  FrameTree& frame_tree() const { return frame_tree_.get(); }
  Navigator& navigator();

  RenderFrameHostManager* render_manager() { return &render_manager_; }
  const RenderFrameHostManager* render_manager() const {
    return &render_manager_;
  }
  FrameTreeNodeId frame_tree_node_id() const { return frame_tree_node_id_; }
  // This reflects window.name, which is initially set to the the "name"
  // attribute. But this won't reflect changes of 'name' attribute and instead
  // reflect changes to the Window object's name property.
  // This is different from IframeAttributes' name in that this will not get
  // updated when 'name' attribute gets updated.
  const std::string& frame_name() const {
    return render_manager_.current_replication_state().name;
  }

  const std::string& unique_name() const {
    return render_manager_.current_replication_state().unique_name;
  }

  size_t child_count() const { return current_frame_host()->child_count(); }

  RenderFrameHostImpl* parent() const { return parent_; }

  // See `RenderFrameHost::GetParentOrOuterDocument()` for
  // documentation.
  RenderFrameHostImpl* GetParentOrOuterDocument() const;

  // See `RenderFrameHostImpl::GetParentOrOuterDocumentOrEmbedder()` for
  // documentation.
  RenderFrameHostImpl* GetParentOrOuterDocumentOrEmbedder();

  FrameTreeNode* opener() const { return opener_; }

  FrameTreeNode* first_live_main_frame_in_original_opener_chain() const {
    return first_live_main_frame_in_original_opener_chain_;
  }

  const std::optional<base::UnguessableToken>& opener_devtools_frame_token() {
    return opener_devtools_frame_token_;
  }

  // Returns the type of the frame. Refer to frame_type.h for the details.
  FrameType GetFrameType() const;

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

  // Assigns an opener frame id for this node. This string id is only set once
  // and cannot be changed. It persists, even if the |opener| is destroyed. It
  // is used for attribution in the DevTools frontend.
  void SetOpenerDevtoolsFrameToken(
      base::UnguessableToken opener_devtools_frame_token);

  FrameTreeNode* child_at(size_t index) const {
    return current_frame_host()->child_at(index);
  }

  // Returns the URL of the last committed page in the current frame.
  const GURL& current_url() const {
    return current_frame_host()->GetLastCommittedURL();
  }

  // Moves this frame out of the initial empty document state, which is a
  // one-way change for FrameTreeNode (i.e., it cannot go back into the initial
  // empty document state).
  void set_not_on_initial_empty_document() {
    is_on_initial_empty_document_ = false;
  }

  // Returns false if the frame has committed a document that is not the initial
  // empty document, or if the current document's input stream has been opened
  // with document.open(), causing the document to lose its "initial empty
  // document" status. For more details, see the definition of
  // `is_on_initial_empty_document_`.
  bool is_on_initial_empty_document() const {
    return is_on_initial_empty_document_;
  }

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
    return render_manager_.current_replication_state().origin;
  }

  // Returns the latest frame policy (sandbox flags and container policy) for
  // this frame. This includes flags inherited from parent frames and the latest
  // flags from the <iframe> element hosting this frame. The returned policies
  // may not yet have taken effect, since "sandbox" and "allow" attribute
  // updates in an <iframe> element take effect on next navigation. For
  // <fencedframe> elements, not everything in the frame policy might actually
  // take effect after the navigation. To retrieve the currently active policy
  // for this frame, use effective_frame_policy().
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
  // the permissions policy container policy, which is set by the iframe's
  // allowfullscreen, allowpaymentrequest, and allow attributes, along with the
  // origin of the iframe's src attribute (which may be different from the URL
  // of the document currently loaded into the frame). This does not include
  // policy changes that have been made by updating the containing iframe
  // element attributes since the frame was last navigated; use
  // pending_frame_policy() for those.
  const blink::FramePolicy& effective_frame_policy() const {
    return render_manager_.current_replication_state().frame_policy;
  }

  const blink::mojom::FrameOwnerProperties& frame_owner_properties() {
    return frame_owner_properties_;
  }

  void set_frame_owner_properties(
      const blink::mojom::FrameOwnerProperties& frame_owner_properties) {
    frame_owner_properties_ = frame_owner_properties;
  }

  // Reflects the attributes of the corresponding iframe html element, such
  // as 'credentialless', 'id', 'name' and 'src'. These values should not be
  // exposed to cross-origin renderers.
  const network::mojom::ContentSecurityPolicy* csp_attribute() const {
    return attributes_->parsed_csp_attribute.get();
  }
  // Tracks iframe's 'browsingtopics' attribute, indicating whether the
  // navigation requests on this frame should calculate and send the
  // `Sec-Browsing-Topics` header.
  bool browsing_topics() const { return attributes_->browsing_topics; }

  // Tracks iframe's 'adauctionheaders' attribute, indicating whether the
  // navigation request on this frame should calculate and send the
  // 'Sec-Ad-Auction-Fetch` header.
  bool ad_auction_headers() const { return attributes_->ad_auction_headers; }

  // Tracks iframe's 'sharedstoragewritable' attribute, indicating what value
  // the the corresponding
  // `network::ResourceRequest::shared_storage_writable_eligible` should take
  // for the navigation(s) on this frame, pending a permissions policy check. If
  // true, and if the permissions policy check returns "enabled", the network
  // service will send the `Shared-Storage-Write` request header.
  bool shared_storage_writable_opted_in() const {
    return attributes_->shared_storage_writable_opted_in;
  }
  const std::optional<std::string> html_id() const { return attributes_->id; }
  // This tracks iframe's 'name' attribute instead of window.name, which is
  // tracked in FrameReplicationState. See the comment for frame_name() for
  // more details.
  const std::optional<std::string> html_name() const {
    return attributes_->name;
  }
  const std::optional<std::string> html_src() const { return attributes_->src; }

  void SetAttributes(blink::mojom::IframeAttributesPtr attributes);

  bool HasSameOrigin(const FrameTreeNode& node) const {
    return render_manager_.current_replication_state().origin.IsSameOriginWith(
        node.current_replication_state().origin);
  }

  const blink::mojom::FrameReplicationState& current_replication_state() const {
    return render_manager_.current_replication_state();
  }

  RenderFrameHostImpl* current_frame_host() const {
    return render_manager_.current_frame_host();
  }

  // Returns true if this node is in a loading state.
  bool IsLoading() const;
  LoadingState GetLoadingState() const;

  // Returns true if this node has a cross-document navigation in progress.
  bool HasPendingCrossDocumentNavigation() const;

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
  void TakeNavigationRequest(
      std::unique_ptr<NavigationRequest> navigation_request);

  // Resets the navigation request owned by `this` (which shouldn't have reached
  // the "pending commit" stage yet) and any state created by it, including the
  // speculative RenderFrameHost (if there are no other navigations associated
  // with it). Note that this does not affect navigations that have reached the
  // "pending commit" stage, which are owned by their corresponding
  // RenderFrameHosts instead.
  void ResetNavigationRequest(NavigationDiscardReason reason);

  // Similar to `ResetNavigationRequest()`, but keeps the state created by the
  // NavigationRequest (e.g. speculative RenderFrameHost, loading state).
  void ResetNavigationRequestButKeepState(NavigationDiscardReason reason);

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

  // Returns the sandbox flags currently in effect for this frame. This includes
  // flags inherited from parent frames, the currently active flags from the
  // <iframe> element hosting this frame, as well as any flags set from a
  // Content-Security-Policy HTTP header. This does not include flags that have
  // have been updated in an <iframe> element but have not taken effect yet; use
  // pending_frame_policy() for those. To see the flags which will take effect
  // on navigation (which does not include the CSP-set flags), use
  // effective_frame_policy().
  network::mojom::WebSandboxFlags active_sandbox_flags() const {
    return render_manager_.current_replication_state().active_sandbox_flags;
  }

  // Returns whether the frame received a user gesture on a previous navigation
  // on the same eTLD+1.
  bool has_received_user_gesture_before_nav() const {
    return render_manager_.current_replication_state()
        .has_received_user_gesture_before_nav;
  }

  // When a tab is discarded, WebContents sets was_discarded on its
  // root FrameTreeNode.
  // In addition, when a child frame is created, this bit is passed on from
  // parent to child.
  // When a navigation request is created, was_discarded is passed on to the
  // request and reset to false in FrameTreeNode.
  void set_was_discarded() { was_discarded_ = true; }
  bool was_discarded() const { return was_discarded_; }

  // Deprecated. Use directly HasStickyUserActivation in RFHI.
  // Returns the sticky bit of the User Activation v2 state of the
  // |FrameTreeNode|.
  bool HasStickyUserActivation() const {
    return current_frame_host()->HasStickyUserActivation();
  }

  // Deprecated. Use directly HasStickyUserActivation in RFHI.
  // Returns the transient bit of the User Activation v2 state of the
  // |FrameTreeNode|.
  bool HasTransientUserActivation() {
    return current_frame_host()->HasTransientUserActivation();
  }

  // Remove history entries for all frames created by script in this frame's
  // subtree. If a frame created by a script is removed, then its history entry
  // will never be reused - this saves memory.
  void PruneChildFrameNavigationEntries(NavigationEntryImpl* entry);

  using FencedFrameStatus = RenderFrameHostImpl::FencedFrameStatus;
  FencedFrameStatus fenced_frame_status() const { return fenced_frame_status_; }

  blink::FrameOwnerElementType frame_owner_element_type() const {
    return frame_owner_element_type_;
  }

  blink::mojom::TreeScopeType tree_scope_type() const {
    return tree_scope_type_;
  }

  // The initial popup URL for new window opened using:
  // `window.open(initial_popup_url)`.
  // An empty GURL otherwise.
  //
  // [WARNING] There is no guarantee the FrameTreeNode will ever host a
  // document served from this URL. The FrameTreeNode always starts hosting the
  // initial empty document and attempts a navigation toward this URL. However
  // the navigation might be delayed, redirected and even cancelled.
  void SetInitialPopupURL(const GURL& initial_popup_url);
  const GURL& initial_popup_url() const { return initial_popup_url_; }

  // The origin of the document that used window.open() to create this frame.
  // Otherwise, an opaque Origin with a nonce different from all previously
  // existing Origins.
  void SetPopupCreatorOrigin(const url::Origin& popup_creator_origin);
  const url::Origin& popup_creator_origin() const {
    return popup_creator_origin_;
  }

  // Sets the associated FrameTree for this node. The node can change FrameTrees
  // as part of prerendering, which allows a page loaded in the prerendered
  // FrameTree to be used for a navigation in the primary frame tree.
  void SetFrameTree(FrameTree& frame_tree);

  using TraceProto = perfetto::protos::pbzero::FrameTreeNodeInfo;
  // Write a representation of this object into a trace.
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> proto) const;

  // Returns true the node is navigating, i.e. it has an associated
  // NavigationRequest.
  bool HasNavigation();

  // Returns true if there are any navigations happening in FrameTreeNode that
  // is pending commit (i.e. between ReadyToCommit and DidCommit). Note that
  // those navigations won't live in the FrameTreeNode itself, as they will
  // already be owned by the committing RenderFrameHost (either the current
  // RenderFrameHost or the speculative RenderFrameHost).
  bool HasPendingCommitNavigation();

  // Fenced frames (meta-bug crbug.com/1111084):
  // Note that these two functions cannot be invoked from a FrameTree's or
  // its root node's constructor since they require the frame tree and the
  // root node to be completely constructed.
  //
  // Returns false if fenced frames are disabled. Returns true if the feature is
  // enabled and if |this| is a fenced frame. Returns false for
  // iframes embedded in a fenced frame. To clarify: for the MPArch
  // implementation this only returns true if |this| is the actual
  // root node of the inner FrameTree and not the proxy FrameTreeNode in the
  // outer FrameTree.
  bool IsFencedFrameRoot() const;

  // Returns false if fenced frames are disabled. Returns true if the
  // feature is enabled and if |this| or any of its ancestor nodes is a
  // fenced frame.
  bool IsInFencedFrameTree() const;

  // Returns a valid nonce if `IsInFencedFrameTree()` returns true for `this`.
  // Returns nullopt otherwise.
  //
  // Nonce used in the net::IsolationInfo and blink::StorageKey for a fenced
  // frame and any iframes nested within it. Not set if this frame is not in a
  // fenced frame's FrameTree. Note that this could be a field in FrameTree for
  // the MPArch version but for the shadow DOM version we need to keep it here
  // since the fenced frame root is not a main frame for the latter. The value
  // of the nonce will be the same for all of the the iframes inside a fenced
  // frame tree. If there is a nested fenced frame it will have a different
  // nonce than its parent fenced frame. The nonce will stay the same across
  // navigations initiated from the fenced frame tree because it is always used
  // in conjunction with other fields of the keys and would be good to access
  // the same storage across same-origin navigations. If the navigation is
  // same-origin/site then the same network stack partition/storage will be
  // reused and if it's cross-origin/site then other parts of the key will
  // change and so, even with the same nonce, another partition will be used.
  // But if the navigation is initiated from the embedder, the nonce will be
  // reinitialized irrespective of same or cross origin such that there is no
  // privacy leak via storage shared between two embedder initiated navigations.
  // Note that this reinitialization is implemented for all embedder-initiated
  // navigations in MPArch, but only urn:uuid navigations in ShadowDOM.
  std::optional<base::UnguessableToken> GetFencedFrameNonce();

  // If applicable, initialize the default fenced frame properties. Right now,
  // this means setting a new fenced frame nonce. See comment on
  // fenced_frame_nonce() for when it is set to a non-null value. Invoked
  // by FrameTree::Init() or FrameTree::AddFrame().
  void SetFencedFramePropertiesIfNeeded();

  // Set the current FencedFrameProperties to have "opaque ads mode".
  // This should only be used during tests, when the proper embedder-initiated
  // fenced frame root urn/config navigation flow isn't available.
  // TODO(crbug.com/40233168): Refactor and expand use of test utils so there is
  // a consistent way to do this properly everywhere. Consider removing
  // arbitrary restrictions in "default mode" so that using opaque ads mode is
  // less necessary.
  void SetFencedFramePropertiesOpaqueAdsModeForTesting() {
    if (fenced_frame_properties_.has_value()) {
      fenced_frame_properties_
          ->SetFencedFramePropertiesOpaqueAdsModeForTesting();
    }
  }

  // Returns the mode attribute from the `FencedFrameProperties` if this frame
  // is in a fenced frame tree, otherwise returns `kDefault`.
  blink::FencedFrame::DeprecatedFencedFrameMode GetDeprecatedFencedFrameMode();

  // Helper for GetParentOrOuterDocument/GetParentOrOuterDocumentOrEmbedder.
  // Do not use directly.
  // `escape_guest_view` determines whether to iterate out of guest views and is
  // the behaviour distinction between GetParentOrOuterDocument and
  // GetParentOrOuterDocumentOrEmbedder. See the comment on
  // GetParentOrOuterDocumentOrEmbedder for details.
  // `include_prospective` includes embedders which own our frame tree, but have
  // not yet attached it to the outer frame tree.
  RenderFrameHostImpl* GetParentOrOuterDocumentHelper(
      bool escape_guest_view,
      bool include_prospective) const;

  // Sets the unique_name and name fields on replication_state_. To be used in
  // prerender activation to make sure the FrameTreeNode replication state is
  // correct after the RenderFrameHost is moved between FrameTreeNodes. The
  // renderers should already have the correct value, so unlike
  // FrameTreeNode::SetFrameName, we do not notify them here.
  // TODO(crbug.com/40192974): Remove this once the BrowsingContextState
  //  is implemented to utilize the new path.
  void set_frame_name_for_activation(const std::string& unique_name,
                                     const std::string& name) {
    current_frame_host()->browsing_context_state()->set_frame_name(unique_name,
                                                                   name);
  }

  // Returns true if error page isolation is enabled.
  bool IsErrorPageIsolationEnabled() const;

  // Functions to store and retrieve a frame's srcdoc value on this
  // FrameTreeNode.
  void SetSrcdocValue(const std::string& srcdoc_value);
  const std::string& srcdoc_value() const { return srcdoc_value_; }

  void set_fenced_frame_properties(
      const std::optional<FencedFrameProperties>& fenced_frame_properties) {
    // TODO(crbug.com/40202462): Reenable this DCHECK once ShadowDOM and
    // loading urns in iframes (for FLEDGE OT) are gone.
    // DCHECK_EQ(fenced_frame_status_,
    //          RenderFrameHostImpl::FencedFrameStatus::kFencedFrameRoot);
    fenced_frame_properties_ = fenced_frame_properties;
  }

  // This function returns the fenced frame properties associated with the given
  // source.
  // - If `source_node` is set to `kClosestAncestor`, the fenced frame
  // properties are obtained by a bottom-up traversal from this node.
  // - If `source_node` is set tp `kFrameTreeRoot`, the fenced frame properties
  // from the fenced frame tree root are returned.
  // For example, for an urn iframe that is nested inside a fenced frame.
  // Calling this function from the nested urn iframe with `source_node` set to:
  // - `kClosestAncestor`: returns the fenced frame properties from the urn
  // iframe.
  // - `kFrameTreeRoot`: returns the fenced frame properties from the fenced
  // frame.
  // Clients should decide which one to use depending on how the application of
  // the fenced frame properties interact with urn iframes.
  // TODO(crbug.com/40060657): Once navigation support for urn::uuid in iframes
  // is deprecated, remove the parameter `node_source`.
  std::optional<FencedFrameProperties>& GetFencedFrameProperties(
      FencedFramePropertiesNodeSource node_source =
          FencedFramePropertiesNodeSource::kClosestAncestor);

  // Helper function for getting the FrameTreeNode that houses the relevant
  // FencedFrameProperties when GetFencedFrameProperties() is called with
  // kClosestAncestor.
  FrameTreeNode* GetClosestAncestorWithFencedFrameProperties();

  bool HasFencedFrameProperties() const {
    return fenced_frame_properties_.has_value();
  }

  // Returns the number of fenced frame boundaries above this frame. The
  // outermost main frame's frame tree has fenced frame depth 0, a topmost
  // fenced frame tree embedded in the outermost main frame has fenced frame
  // depth 1, etc.
  //
  // Also, sets `shared_storage_fenced_frame_root_count` to the
  // number of fenced frame boundaries (roots) above this frame that originate
  // from shared storage. This is used to check whether a fenced frame
  // originates from shared storage only (i.e. not from FLEDGE).
  // TODO(crbug.com/40233168): Remove this check once we put permissions inside
  // FencedFrameConfig.
  size_t GetFencedFrameDepth(size_t& shared_storage_fenced_frame_root_count);

  // Traverse up from this node. Return all valid
  // `node->fenced_frame_properties_->shared_storage_budget_metadata` (i.e. this
  // node is subjected to the shared storage budgeting associated with those
  // metadata). Every node that originates from sharedStorage.selectURL() will
  // have an associated metadata. This indicates that the metadata can only
  // possibly be associated with a fenced frame root, unless when
  // `kAllowURNsInIframes` is enabled in which case they could be be associated
  // with any node.
  std::vector<const SharedStorageBudgetMetadata*>
  FindSharedStorageBudgetMetadata();

  // Returns any shared storage context string that was written to a
  // `blink::FencedFrameConfig` before navigation via
  // `setSharedStorageContext()`, as long as the request is for a same-origin
  // frame within the config's fenced frame tree (or a same-origin descendant of
  // a URN iframe).
  std::optional<std::u16string> GetEmbedderSharedStorageContextIfAllowed();

  // Accessor to BrowsingContextState for subframes only. Only main frame
  // navigations can change BrowsingInstances and BrowsingContextStates,
  // therefore for subframes associated BrowsingContextState never changes. This
  // helper method makes this more explicit and guards against calling this on
  // main frames (there an appropriate BrowsingContextState should be obtained
  // from RenderFrameHost or from RenderFrameProxyHost as e.g. during
  // cross-BrowsingInstance navigations multiple BrowsingContextStates exist in
  // the same frame).
  const scoped_refptr<BrowsingContextState>&
  GetBrowsingContextStateForSubframe() const;

  // Clears the opener property of popups referencing this FrameTreeNode as
  // their opener.
  void ClearOpenerReferences();

  // Calculates whether one of the ancestor frames or this frame has a CSPEE in
  // place. This is eventually sent over to LocalFrame in the renderer where it
  // will be used by NavigatorAuction::canLoadAdAuctionFencedFrame for
  // information it can't get on its own.
  bool AncestorOrSelfHasCSPEE() const;

  // Reset every navigation in this frame, and its descendants. This is called
  // after the <iframe> element has been removed, or after the document owning
  // this frame has been navigated away.
  //
  // This takes into account:
  // - Non-pending commit NavigationRequest owned by the FrameTreeNode
  // - Pending commit NavigationRequest owned by the current RenderFrameHost
  // - Speculative RenderFrameHost and its pending commit NavigationRequests.
  void ResetAllNavigationsForFrameDetach();

  // RenderFrameHostOwner implementation:
  void DidStartLoading(LoadingState previous_frame_tree_loading_state) override;
  void DidStopLoading() override;
  void RestartNavigationAsCrossDocument(
      std::unique_ptr<NavigationRequest> navigation_request) override;
  bool Reload() override;
  Navigator& GetCurrentNavigator() override;
  RenderFrameHostManager& GetRenderFrameHostManager() override;
  FrameTreeNode* GetOpener() const override;
  void SetFocusedFrame(SiteInstanceGroup* source) override;
  void DidChangeReferrerPolicy(
      network::mojom::ReferrerPolicy referrer_policy) override;
  // Updates the user activation state in the browser frame tree and in the
  // frame trees in all renderer processes except the renderer for this node
  // (which initiated the update).  Returns |false| if the update tries to
  // consume an already consumed/expired transient state, |true| otherwise.  See
  // the comment on `user_activation_state_` in RenderFrameHostImpl.
  //
  // The |notification_type| parameter is used for histograms, only for the case
  // |update_state == kNotifyActivation|.
  bool UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType update_type,
      blink::mojom::UserActivationNotificationType notification_type) override;
  void DidConsumeHistoryUserActivation() override;
  void DidOpenDocumentInputStream() override;
  std::unique_ptr<NavigationRequest>
  CreateNavigationRequestForSynchronousRendererCommit(
      RenderFrameHostImpl* render_frame_host,
      bool is_same_document,
      const GURL& url,
      const url::Origin& origin,
      const std::optional<GURL>& initiator_base_url,
      const net::IsolationInfo& isolation_info_for_subresources,
      blink::mojom::ReferrerPtr referrer,
      const ui::PageTransition& transition,
      bool should_replace_current_entry,
      const std::string& method,
      bool has_transient_activation,
      bool is_overriding_user_agent,
      const std::vector<GURL>& redirects,
      const GURL& original_url,
      std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter,
      int http_response_code) override;
  void CancelNavigation(NavigationDiscardReason reason) override;
  void ResetNavigationsForDiscard() override;
  bool Credentialless() const override;
#if !BUILDFLAG(IS_ANDROID)
  void GetVirtualAuthenticatorManager(
      mojo::PendingReceiver<blink::test::mojom::VirtualAuthenticatorManager>
          receiver) override;
#endif
  FrameType GetCurrentFrameType() const override;

  // Restart the navigation restoring the page from the back-forward cache
  // as a regular non-BFCached history navigation.
  //
  // The restart itself is asynchronous as it's dangerous to restart navigation
  // with arbitrary state on the stack (another navigation might be starting),
  // so this function only posts the actual task to do all the work (See
  // `RestartBackForwardCachedNavigationImpl()`).
  void RestartBackForwardCachedNavigationAsync(int nav_entry_id);

  // Cancel the asynchronous task that would restart the BFCache navigation.
  // This should be called whenever a FrameTreeNode's NavigationRequest would
  // normally get cancelled, including when another NavigationRequest starts.
  // This preserves the previous behavior where a restarting BFCache
  // NavigationRequest is kept around until the task to create the new
  // navigation is run, or until that NavigationRequest gets deleted (which
  // cancels the task).
  void CancelRestartingBackForwardCacheNavigation();

  base::SafeRef<FrameTreeNode> GetSafeRef() {
    return weak_factory_.GetSafeRef();
  }

 private:
  friend class CSPEmbeddedEnforcementUnitTest;
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessPermissionsPolicyBrowserTest,
                           ContainerPolicyDynamic);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessPermissionsPolicyBrowserTest,
                           ContainerPolicySandboxDynamic);
  FRIEND_TEST_ALL_PREFIXES(NavigationRequestTest, StorageKeyToCommit);
  FRIEND_TEST_ALL_PREFIXES(
      NavigationRequestTest,
      NavigationToCredentiallessDocumentNetworkIsolationInfo);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostImplTest,
                           ChildOfCredentiallessIsCredentialless);
  FRIEND_TEST_ALL_PREFIXES(ContentPasswordManagerDriverTest,
                           PasswordAutofillDisabledOnCredentiallessIframe);

  // Called by the destructor. When `this` is an outer dummy FrameTreeNode
  // representing an inner FrameTree, this method destroys said inner FrameTree.
  void DestroyInnerFrameTreeIfExists();

  class OpenerDestroyedObserver;

  // The |notification_type| parameter is used for histograms only.
  // |sticky_only| is set to true when propagating sticky user activation during
  // cross-document navigations. The transient state remains unchanged.
  bool NotifyUserActivation(
      blink::mojom::UserActivationNotificationType notification_type,
      bool sticky_only = false);

  bool NotifyUserActivationStickyOnly();

  bool ConsumeTransientUserActivation();

  bool ClearUserActivation();

  // Verify that the renderer process is allowed to set user activation on this
  // frame by checking whether this frame's RenderWidgetHost had previously seen
  // an input event that might lead to user activation. If user activation
  // should be allowed, this returns true and also clears corresponding pending
  // user activation state in the widget. Otherwise, this returns false.
  bool VerifyUserActivation();

  // See `RestartBackForwardCachedNavigationAsync()`.
  void RestartBackForwardCachedNavigationImpl(int nav_entry_id);

  // The browser-global FrameTreeNodeId generator.
  static FrameTreeNodeId::Generator frame_tree_node_id_generator_;

  // The FrameTree owning |this|. It can change with Prerender2 during
  // activation.
  raw_ref<FrameTree> frame_tree_;

  // A browser-global identifier for the frame in the page, which stays stable
  // even if the frame does a cross-process navigation.
  const FrameTreeNodeId frame_tree_node_id_;

  // The RenderFrameHost owning this FrameTreeNode, which cannot change for the
  // life of this FrameTreeNode. |nullptr| if this node is the root.
  const raw_ptr<RenderFrameHostImpl> parent_;

  // The frame that opened this frame, if any.  Will be set to null if the
  // opener is closed, or if this frame disowns its opener by setting its
  // window.opener to null.
  raw_ptr<FrameTreeNode> opener_ = nullptr;

  // An observer that clears this node's |opener_| if the opener is destroyed.
  // This observer is added to the |opener_|'s observer list when the |opener_|
  // is set to a non-null node, and it is removed from that list when |opener_|
  // changes or when this node is destroyed.  It is also cleared if |opener_|
  // is disowned.
  std::unique_ptr<OpenerDestroyedObserver> opener_observer_;

  // Unlike `opener_`, the "original opener chain" doesn't reflect
  // window.opener, which can be suppressed or updated. The "original opener"
  // is the main frame of the actual opener of this frame. This traces the all
  // the way back, so if the original opener was closed (deleted or severed due
  // to COOP), but _it_ had an original opener, this will return the original
  // opener's original opener, etc. So this value will always be set as long as
  // there is at least one live frame in the chain whose connection is not
  // severed due to COOP.
  raw_ptr<FrameTreeNode> first_live_main_frame_in_original_opener_chain_ =
      nullptr;

  // The devtools frame token of the frame which opened this frame. This is
  // not cleared even if the opener is destroyed or disowns the frame.
  std::optional<base::UnguessableToken> opener_devtools_frame_token_;

  // An observer that updates this node's
  // |first_live_main_frame_in_original_opener_chain_| to the next original
  // opener in the chain if the original opener is destroyed.
  std::unique_ptr<OpenerDestroyedObserver> original_opener_observer_;

  // When created by an opener, the URL specified in window.open(url)
  // Please refer to {Get,Set}InitialPopupURL() documentation.
  GURL initial_popup_url_;

  // When created using window.open, the origin of the creator.
  // Please refer to {Get,Set}PopupCreatorOrigin() documentation.
  url::Origin popup_creator_origin_;

  // If the url from the the last BeginNavigation is about:srcdoc, this value
  // stores the srcdoc_attribute's value for re-use in history navigations.
  std::string srcdoc_value_;

  // Whether this frame is still on the initial about:blank document or the
  // synchronously committed about:blank document committed at frame creation,
  // and its "initial empty document"-ness is still true.
  // This will be false if either of these has happened:
  // - The current RenderFrameHost commits a cross-document navigation that is
  //   not the synchronously committed about:blank document per:
  //   https://html.spec.whatwg.org/multipage/browsers.html#creating-browsing-contexts:is-initial-about:blank
  // - The document's input stream has been opened with document.open(), per
  //   https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#opening-the-input-stream:is-initial-about:blank
  // NOTE: we treat both the "initial about:blank document" and the
  // "synchronously committed about:blank document" as the initial empty
  // document. In the future, we plan to remove the synchronous about:blank
  // commit so that this state will only be true if the frame is on the
  // "initial about:blank document". See also:
  // - https://github.com/whatwg/html/issues/6863
  // - https://crbug.com/1215096
  //
  // Note that cross-document navigations update this state at
  // DidCommitNavigation() time. Thus, this is still true when a cross-document
  // navigation from an initial empty document is in the pending-commit window,
  // after sending the CommitNavigation IPC but before receiving
  // DidCommitNavigation().  This is in contrast to
  // has_committed_any_navigation(), which is updated in CommitNavigation().
  // TODO(alexmos): Consider updating this at CommitNavigation() time as well to
  // match the has_committed_any_navigation() behavior.
  bool is_on_initial_empty_document_ = true;

  // Whether the frame's owner element in the parent document is collapsed.
  bool is_collapsed_ = false;

  // The type of frame owner for this frame. This is only relevant for non-main
  // frames.
  const blink::FrameOwnerElementType frame_owner_element_type_ =
      blink::FrameOwnerElementType::kNone;

  // The tree scope type of frame owner element, i.e. whether the element is in
  // the document tree (https://dom.spec.whatwg.org/#document-trees) or the
  // shadow tree (https://dom.spec.whatwg.org/#shadow-trees). This is only
  // relevant for non-main frames.
  const blink::mojom::TreeScopeType tree_scope_type_ =
      blink::mojom::TreeScopeType::kDocument;

  // Track the pending sandbox flags and container policy for this frame. When a
  // parent frame dynamically updates 'sandbox', 'allow', 'allowfullscreen',
  // 'allowpaymentrequest' or 'src' attributes, the updated policy for the frame
  // is stored here, and transferred into
  // render_manager_.current_replication_state().frame_policy when they take
  // effect on the next frame navigation.
  blink::FramePolicy pending_frame_policy_;

  // Whether the frame was created by javascript.  This is useful to prune
  // history entries when the frame is removed (because frames created by
  // scripts are never recreated with the same unique name - see
  // https://crbug.com/500260).
  const bool is_created_by_script_;

  // Tracks the scrolling and margin properties for this frame.  These
  // properties affect the child renderer but are stored on its parent's
  // frame element.  When this frame's parent dynamically updates these
  // properties, we update them here too.
  //
  // Note that dynamic updates only take effect on the next frame navigation.
  blink::mojom::FrameOwnerProperties frame_owner_properties_;

  // Contains the tracked HTML attributes of the corresponding iframe element,
  // such as 'id' and 'src'.
  blink::mojom::IframeAttributesPtr attributes_;

  // Owns an ongoing NavigationRequest until it is ready to commit. It will then
  // be reset and a RenderFrameHost will be responsible for the navigation.
  std::unique_ptr<NavigationRequest> navigation_request_;

  // List of objects observing this FrameTreeNode.
  base::ObserverList<Observer>::Unchecked observers_;

  base::TimeTicks last_focus_time_;

  bool was_discarded_ = false;

  const FencedFrameStatus fenced_frame_status_ =
      FencedFrameStatus::kNotNestedInFencedFrame;

  // If this is a fenced frame resulting from a urn:uuid navigation, this
  // contains all the metadata specifying the resulting context.
  // TODO(crbug.com/40202462): Move this into the FrameTree once ShadowDOM
  // and urn iframes are gone.
  std::optional<FencedFrameProperties> fenced_frame_properties_;

  // The tracker of the task that restarts the BFCache navigation. It might be
  // used to cancel the task.
  // See `CancelRestartingBackForwardCacheNavigation()`.
  base::CancelableTaskTracker restart_back_forward_cached_navigation_tracker_;

  // Manages creation and swapping of RenderFrameHosts for this frame.
  //
  // This field needs to be declared last, because destruction of
  // RenderFrameHostManager may call arbitrary callbacks (e.g. via
  // WebContentsObserver::DidFinishNavigation fired after RenderFrameHostManager
  // destructs a RenderFrameHostImpl and its NavigationRequest).  Such callbacks
  // may try to use FrameTreeNode's fields above - this would be an undefined
  // behavior if the fields (even trivially-destructible ones) were destructed
  // before the RenderFrameHostManager's destructor runs.  See also
  // https://crbug.com/1157988.
  RenderFrameHostManager render_manager_;

  base::WeakPtrFactory<FrameTreeNode> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_TREE_NODE_H_
