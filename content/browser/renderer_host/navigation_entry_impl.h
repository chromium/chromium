// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_ENTRY_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_ENTRY_IMPL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_data.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/replaced_navigation_entry_data.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/ssl_status.h"
#include "net/base/isolation_info.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/system_entropy.mojom-forward.h"
#include "url/origin.h"

namespace blink {
struct FramePolicy;
namespace scheduler {
class TaskAttributionId;
}  // namespace scheduler
}  // namespace blink

namespace content {

class FrameTreeNode;
class NavigationEntryRestoreContext;
class NavigationEntryRestoreContextImpl;

class CONTENT_EXPORT NavigationEntryImpl : public NavigationEntry {
 public:
  // Determines whether CloneAndReplace will share the existing
  // FrameNavigationEntries in the new NavigationEntry or not.
  enum class ClonePolicy { kShareFrameEntries, kCloneFrameEntries };

  // Represents a tree of FrameNavigationEntries that make up this joint session
  // history item.
  struct TreeNode {
    TreeNode(TreeNode* parent, scoped_refptr<FrameNavigationEntry> frame_entry);
    ~TreeNode();

    // Returns whether this TreeNode corresponds to |frame_tree_node|.  If this
    // is called on the root TreeNode, we only check if |frame_tree_node| is the
    // main frame.  Otherwise, we check if the unique name matches.
    bool MatchesFrame(FrameTreeNode* frame_tree_node) const;

    // Recursively makes a copy of this TreeNode, either sharing
    // FrameNavigationEntries or making deep copies depending on |clone_policy|.
    // Replaces the TreeNode corresponding to |target_frame_tree_node|,
    // clearing all of its children unless |clone_children_of_target| is true.
    // This function omits any subframe history items that do not correspond to
    // frames actually in the current page, using |current_frame_tree_node| (if
    // present). |restore_context| is used to keep track of the
    // FrameNavigationEntries that have been created during a deep clone, and to
    // ensure that multiple copies of the same FrameNavigationEntry in different
    // NavigationEntries are de-duplicated.
    std::unique_ptr<TreeNode> CloneAndReplace(
        scoped_refptr<FrameNavigationEntry> frame_navigation_entry,
        bool clone_children_of_target,
        FrameTreeNode* target_frame_tree_node,
        FrameTreeNode* current_frame_tree_node,
        TreeNode* parent_node,
        NavigationEntryRestoreContextImpl* restore_context,
        ClonePolicy clone_policy) const;

    // The parent of this node.
    raw_ptr<TreeNode> parent;

    // Ref counted pointer that keeps the FrameNavigationEntry alive as long as
    // it is needed by this node's NavigationEntry.
    scoped_refptr<FrameNavigationEntry> frame_entry;

    // List of child TreeNodes, which will be deleted when this node is.
    std::vector<std::unique_ptr<TreeNode>> children;
  };

  static NavigationEntryImpl* FromNavigationEntry(NavigationEntry* entry);
  static const NavigationEntryImpl* FromNavigationEntry(
      const NavigationEntry* entry);
  static std::unique_ptr<NavigationEntryImpl> FromNavigationEntry(
      std::unique_ptr<NavigationEntry> entry);

  NavigationEntryImpl();
  NavigationEntryImpl(
      scoped_refptr<SiteInstanceImpl> instance,
      const GURL& url,
      const Referrer& referrer,
      const std::optional<url::Origin>& initiator_origin,
      const std::optional<GURL>& initiator_base_url,
      const std::u16string& title,
      ui::PageTransition transition_type,
      bool is_renderer_initiated,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      bool is_initial_entry);

  NavigationEntryImpl(const NavigationEntryImpl&) = delete;
  NavigationEntryImpl& operator=(const NavigationEntryImpl&) = delete;

  ~NavigationEntryImpl() override;

  // NavigationEntry implementation:
  bool IsInitialEntry() override;
  int GetUniqueID() override;
  PageType GetPageType() override;
  void SetURL(const GURL& url) override;
  const GURL& GetURL() override;
  void SetBaseURLForDataURL(const GURL& url) override;
  const GURL& GetBaseURLForDataURL() override;
#if BUILDFLAG(IS_ANDROID)
  void SetDataURLAsString(
      scoped_refptr<base::RefCountedString> data_url) override;
  const scoped_refptr<const base::RefCountedString>& GetDataURLAsString()
      override;
#endif
  void SetReferrer(const Referrer& referrer) override;
  const Referrer& GetReferrer() override;
  void SetVirtualURL(const GURL& url) override;
  const GURL& GetVirtualURL() override;
  void SetTitle(std::u16string title) override;
  const std::u16string& GetTitle() override;
  void SetAppTitle(const std::u16string& app_title) override;
  const std::optional<std::u16string>& GetAppTitle() override;
  void SetPageState(const blink::PageState& state,
                    NavigationEntryRestoreContext* context) override;
  blink::PageState GetPageState() override;
  const std::u16string& GetTitleForDisplay() override;
  bool IsViewSourceMode() override;
  void SetTransitionType(ui::PageTransition transition_type) override;
  ui::PageTransition GetTransitionType() override;
  const GURL& GetUserTypedURL() override;
  void SetHasPostData(bool has_post_data) override;
  bool GetHasPostData() override;
  void SetPostID(int64_t post_id) override;
  int64_t GetPostID() override;
  void SetPostData(
      const scoped_refptr<network::ResourceRequestBody>& data) override;
  scoped_refptr<network::ResourceRequestBody> GetPostData() override;
  FaviconStatus& GetFavicon() override;
  SSLStatus& GetSSL() override;
  void SetOriginalRequestURL(const GURL& original_url) override;
  const GURL& GetOriginalRequestURL() override;
  void SetIsOverridingUserAgent(bool override_ua) override;
  bool GetIsOverridingUserAgent() override;
  void SetTimestamp(base::Time timestamp) override;
  base::Time GetTimestamp() override;
  void SetCanLoadLocalResources(bool allow) override;
  bool GetCanLoadLocalResources() override;
  void SetHttpStatusCode(int http_status_code) override;
  int GetHttpStatusCode() override;
  void SetRedirectChain(const std::vector<GURL>& redirects) override;
  const std::vector<GURL>& GetRedirectChain() override;
  const std::optional<ReplacedNavigationEntryData>& GetReplacedEntryData()
      override;
  bool IsRestored() override;
  std::string GetExtraHeaders() override;
  void AddExtraHeaders(const std::string& extra_headers) override;
  int64_t GetMainFrameDocumentSequenceNumber() override;

  // Creates a copy of this NavigationEntryImpl that can be modified
  // independently from the original, but that shares FrameNavigationEntries.
  // Does not copy any value that would be cleared in ResetForCommit.  Unlike
  // |CloneAndReplace|, this does not check whether the subframe history items
  // are for frames that are still in the current page.
  std::unique_ptr<NavigationEntryImpl> Clone() const;

  // Creates a true deep copy of this NavigationEntryImpl. The
  // FrameNavigationEntries are cloned rather than merely taking a refptr to the
  // original.
  // |restore_context| is used when cloning a vector of NavigationEntryImpls to
  // ensure that FrameNavigationEntries that are shared across multiple entries
  // retain that relationship in the cloned entries.
  std::unique_ptr<NavigationEntryImpl> CloneWithoutSharing(
      NavigationEntryRestoreContextImpl* restore_context) const;

  // Like |Clone|, but replaces the FrameNavigationEntry corresponding to
  // |target_frame_tree_node| with |frame_entry|, clearing all of its children
  // unless |clone_children_of_target| is true.  This function omits any
  // subframe history items that do not correspond to frames actually in the
  // current page, using |root_frame_tree_node| (if present).
  std::unique_ptr<NavigationEntryImpl> CloneAndReplace(
      scoped_refptr<FrameNavigationEntry> frame_entry,
      bool clone_children_of_target,
      FrameTreeNode* target_frame_tree_node,
      FrameTreeNode* root_frame_tree_node) const;

  // Helper functions to construct NavigationParameters for a navigation to this
  // NavigationEntry.
  blink::mojom::CommonNavigationParamsPtr ConstructCommonNavigationParams(
      const FrameNavigationEntry& frame_entry,
      const scoped_refptr<network::ResourceRequestBody>& post_body,
      const GURL& dest_url,
      blink::mojom::ReferrerPtr dest_referrer,
      blink::mojom::NavigationType navigation_type,
      base::TimeTicks navigation_start,
      base::TimeTicks input_start);
  blink::mojom::CommitNavigationParamsPtr ConstructCommitNavigationParams(
      const FrameNavigationEntry& frame_entry,
      const GURL& original_url,
      const std::string& original_method,
      const base::flat_map<std::string, bool>& subframe_unique_names,
      bool intended_as_new_entry,
      int pending_offset_to_send,
      int current_offset_to_send,
      int current_length_to_send,
      const blink::FramePolicy& frame_policy,
      bool ancestor_or_self_has_cspee,
      blink::mojom::SystemEntropy system_entropy_at_navigation_start,
      std::optional<blink::scheduler::TaskAttributionId>
          soft_navigation_heuristics_task_id);

  // Once a navigation entry is committed, we should no longer track several
  // pieces of non-persisted state, as documented on the members below.
  // |frame_entry| is the FrameNavigationEntry for the frame that committed
  // the navigation. It can be null.
  void ResetForCommit(FrameNavigationEntry* frame_entry);

  // Exposes the tree of FrameNavigationEntries that make up this joint session
  // history item.
  TreeNode* root_node() const { return frame_tree_.get(); }

  // Finds the TreeNode associated with |frame_tree_node|, if any.
  NavigationEntryImpl::TreeNode* GetTreeNode(
      FrameTreeNode* frame_tree_node) const;

  // Finds the TreeNode associated with |frame_tree_node_id| to add or update
  // its FrameNavigationEntry.  A new FrameNavigationEntry is added if none
  // exists, or else the existing one (which might be shared with other
  // NavigationEntries) is updated or replaced (based on |update_policy|) with
  // the given parameters.
  // Does nothing if there is no entry already and |url| is about:blank, since
  // that does not count as a real commit.
  enum class UpdatePolicy { kUpdate, kReplace };
  void AddOrUpdateFrameEntry(
      FrameTreeNode* frame_tree_node,
      UpdatePolicy update_policy,
      int64_t item_sequence_number,
      int64_t document_sequence_number,
      const std::string& navigation_api_key,
      SiteInstanceImpl* site_instance,
      scoped_refptr<SiteInstanceImpl> source_site_instance,
      const GURL& url,
      const std::optional<url::Origin>& origin,
      const Referrer& referrer,
      const std::optional<url::Origin>& initiator_origin,
      const std::optional<GURL>& initiator_base_url,
      const std::vector<GURL>& redirect_chain,
      const blink::PageState& page_state,
      const std::string& method,
      int64_t post_id,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
      std::unique_ptr<PolicyContainerPolicies> policy_container_policies);

  // Returns the FrameNavigationEntry corresponding to |frame_tree_node|, if
  // there is one in this NavigationEntry.
  FrameNavigationEntry* GetFrameEntry(FrameTreeNode* frame_tree_node) const;

  // Calls |on_frame_entry| for each FrameNavigationEntry in this
  // NavigationEntry. More efficient than calling GetFrameEntry() N times while
  // iterating over the current tree of FrameTreeNodes.
  using FrameEntryIterationCallback =
      base::FunctionRef<void(FrameNavigationEntry*)>;
  void ForEachFrameEntry(FrameEntryIterationCallback on_frame_entry);

  // Returns a map of frame unique names to |is_about_blank| for immediate
  // children of the TreeNode associated with |frame_tree_node|.  The renderer
  // process will use this list of names to know whether to ask the browser
  // process for a history item when new subframes are created during a
  // back/forward navigation.  (|is_about_blank| can be used to skip the request
  // if the frame's default URL is about:blank and the history item would be a
  // no-op.  See https://crbug.com/657896.)
  // TODO(creis): Send a data structure that also contains all corresponding
  // same-process PageStates for the whole subtree, so that the renderer process
  // only needs to ask the browser process to handle the cross-process cases.
  // See https://crbug.com/639842.
  base::flat_map<std::string, bool> GetSubframeUniqueNames(
      FrameTreeNode* frame_tree_node) const;

  // Walks the tree of FrameNavigationEntries to find entries with |origin| so
  // their isolation status can be registered.
  void RegisterExistingOriginAsHavingDefaultIsolation(
      const url::Origin& origin);

  // Removes any subframe FrameNavigationEntries that match the unique name of
  // |frame_tree_node|, and all of their children. There should be at most one,
  // since collisions are avoided but leave old FrameNavigationEntries in the
  // tree after their frame has been detached.
  //
  // If |only_if_different_position| is specified, then the removal is only
  // done if the found FNE is in a different tree position than the
  // |frame_tree_node|.
  void RemoveEntryForFrame(FrameTreeNode* frame_tree_node,
                           bool only_if_different_position);

  // Update NotRestoredReasons for |navigation_request| which should be a
  // cross-document main frame navigation and is not served from back/forward
  // cache. This will create a metrics object if there is none, which can happen
  // when doing a session restore.
  void UpdateBackForwardCacheNotRestoredReasons(
      NavigationRequest* navigation_request);

  void set_unique_id(int unique_id) { unique_id_ = unique_id; }

  void set_started_from_context_menu(bool started_from_context_menu) {
    started_from_context_menu_ = started_from_context_menu;
  }

  bool has_started_from_context_menu() const {
    return started_from_context_menu_;
  }

  // The SiteInstance represents which pages must share processes. This is a
  // reference counted pointer to a shared SiteInstance.
  //
  // Note that the SiteInstance should usually not be changed after it is set,
  // but this may happen if the NavigationEntry was cloned and needs to use a
  // different SiteInstance.
  SiteInstanceImpl* site_instance() const {
    return frame_tree_->frame_entry->site_instance();
  }

  // The |source_site_instance| is used to identify the SiteInstance of the
  // frame that initiated the navigation. It is set on the
  // FrameNavigationEntry for the main frame.
  void set_source_site_instance(
      scoped_refptr<SiteInstanceImpl> source_site_instance) {
    root_node()->frame_entry->set_source_site_instance(
        source_site_instance.get());
  }

  void set_page_type(PageType page_type) { page_type_ = page_type; }

  bool has_virtual_url() const { return !virtual_url_.is_empty(); }

  bool update_virtual_url_with_url() const {
    return update_virtual_url_with_url_;
  }
  void set_update_virtual_url_with_url(bool update) {
    update_virtual_url_with_url_ = update;
  }

  // Extra headers (separated by \r\n) to send during the request.
  void set_extra_headers(const std::string& extra_headers) {
    extra_headers_ = extra_headers;
  }
  const std::string& extra_headers() const { return extra_headers_; }

  // Whether this (pending) navigation is renderer-initiated.  Resets to false
  // for all types of navigations after commit.
  void set_is_renderer_initiated(bool is_renderer_initiated) {
    is_renderer_initiated_ = is_renderer_initiated;
  }
  bool is_renderer_initiated() const { return is_renderer_initiated_; }

  void set_user_typed_url(const GURL& user_typed_url) {
    user_typed_url_ = user_typed_url;
  }

  // The RestoreType for this entry. This is set if the entry was restored. This
  // is set to RestoreType::NONE once the entry is loaded.
  void set_restore_type(RestoreType type) { restore_type_ = type; }
  RestoreType restore_type() const { return restore_type_; }

  // The ReloadType for this entry.  This is set when a reload is requested.
  // This is set to ReloadType::NONE if the entry isn't for a reload, or once
  // the entry is loaded.
  void set_reload_type(ReloadType type) { reload_type_ = type; }
  ReloadType reload_type() const { return reload_type_; }

  // Whether this (pending) navigation should clear the session history. Resets
  // to false after commit.
  bool should_clear_history_list() const { return should_clear_history_list_; }
  void set_should_clear_history_list(bool should_clear_history_list) {
    should_clear_history_list_ = should_clear_history_list;
  }

  // Indicates which FrameTreeNode to navigate.  Currently only used if the
  // --site-per-process flag is passed.
  FrameTreeNodeId frame_tree_node_id() const { return frame_tree_node_id_; }
  void set_frame_tree_node_id(FrameTreeNodeId frame_tree_node_id) {
    frame_tree_node_id_ = frame_tree_node_id;
  }

  // Returns the history URL for a data URL to use in Blink.
  GURL GetHistoryURLForDataURL();

  // These flags are set when the navigation controller gets notified of an SSL
  // error while a navigation is pending.
  void set_ssl_error(bool error) { ssl_error_ = error; }
  bool ssl_error() const { return ssl_error_; }

  bool has_user_gesture() const { return has_user_gesture_; }

  void set_has_user_gesture(bool has_user_gesture) {
    has_user_gesture_ = has_user_gesture;
  }

  void set_isolation_info(const net::IsolationInfo& isolation_info) {
    isolation_info_ = isolation_info;
  }

  const std::optional<net::IsolationInfo>& isolation_info() const {
    return isolation_info_;
  }

  // Stores a record of the what was committed in this NavigationEntry's main
  // frame before it was replaced (e.g. by history.replaceState()).
  void set_replaced_entry_data(const ReplacedNavigationEntryData& data) {
    replaced_entry_data_ = data;
  }

  // See comment for should_skip_on_back_forward_ui_.
  bool should_skip_on_back_forward_ui() const {
    return should_skip_on_back_forward_ui_;
  }

  void set_should_skip_on_back_forward_ui(bool should_skip) {
    should_skip_on_back_forward_ui_ = should_skip;
  }

  BackForwardCacheMetrics* back_forward_cache_metrics() {
    return back_forward_cache_metrics_.get();
  }

  scoped_refptr<BackForwardCacheMetrics> TakeBackForwardCacheMetrics() {
    return std::move(back_forward_cache_metrics_);
  }

  void set_back_forward_cache_metrics(
      scoped_refptr<BackForwardCacheMetrics> metrics) {
    DCHECK(metrics);
    DCHECK(!back_forward_cache_metrics_);
    back_forward_cache_metrics_ = metrics;
  }

  // Whether this NavigationEntry is the initial NavigationEntry or not, and
  // whether it's for the initial empty document or the synchronously
  // committed about:blank document. The original initial NavigationEntry is
  // created when the FrameTree is created, so it might not be associated with
  // any navigation, but represents a placeholder NavigationEntry for the
  // "initial empty document", which commits in the renderer on frame creation
  // but doesn't notify the browser of the commit. However, more initial
  // NavigationEntries might be created after that in response to navigations,
  // and update or replace the original NavigationEntry. The initial
  // NavigationEntry will only get replaced with a non-initial NavigationEntry
  // by the first navigation that satisfies all of the following conditions:
  //   1. Happens on the main frame
  //   2. Classified as NEW_ENTRY (won't reuse the NavigationEntry)
  //   3. Is not the synchronous about:blank commit
  // So the "initial" status will be retained/copied to the new
  // NavigationEntry on subframe navigations, or when the NavigationEntry is
  // reused/classified as EXISTING_ENTRY (same-document navigations,
  // renderer-initiated reloads), or on the synchronous about:blank commit.
  // Some other important properties of initial NavigationEntries:
  // - The initial NavigationEntry always gets reused or replaced on the next
  // navigation (potentially by another initial NavigationEntry), so if there
  // is an initial NavigationEntry in the session history, it must be the only
  // NavigationEntry (as it is impossible to append to session history if the
  // initial NavigationEntry exists), which means it's not possible to do
  // a history navigation to an initial NavigationEntry.
  // - The initial NavigationEntry never gets restored on session restore,
  // because we never restore tabs with only the initial NavigationEntry.
  enum class InitialNavigationEntryState {
    // An initial NavigationEntry for the initial empty document or a
    // renderer-reloaded initial empty document.
    kInitialNotForSynchronousAboutBlank,
    // An initial NavigationEntry for the synchronously committed about:blank
    // document.
    kInitialForSynchronousAboutBlank,
    // Not an initial NavigationEntry.
    kNonInitial
  };

  bool IsInitialEntryNotForSynchronousAboutBlank() {
    return initial_navigation_entry_state_ ==
           InitialNavigationEntryState::kInitialNotForSynchronousAboutBlank;
  }

  bool IsInitialEntryForSynchronousAboutBlank() {
    return initial_navigation_entry_state_ ==
           InitialNavigationEntryState::kInitialForSynchronousAboutBlank;
  }

  void set_initial_navigation_entry_state(
      InitialNavigationEntryState initial_navigation_entry_state) {
    initial_navigation_entry_state_ = initial_navigation_entry_state;
  }

  InitialNavigationEntryState initial_navigation_entry_state() {
    return initial_navigation_entry_state_;
  }

  NavigationTransitionData& navigation_transition_data() {
    return navigation_transition_data_;
  }
  const NavigationTransitionData& navigation_transition_data() const {
    return navigation_transition_data_;
  }

 private:
  std::unique_ptr<NavigationEntryImpl> CloneAndReplaceInternal(
      scoped_refptr<FrameNavigationEntry> frame_entry,
      bool clone_children_of_target,
      FrameTreeNode* target_frame_tree_node,
      FrameTreeNode* root_frame_tree_node,
      NavigationEntryRestoreContextImpl* restore_context,
      ClonePolicy clone_policy) const;

  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
  // Session/Tab restore save portions of this class so that it can be recreated
  // later. If you add a new field that needs to be persisted you'll have to
  // update SessionService/TabRestoreService and Android WebView
  // state_serializer.cc appropriately.
  // For all new fields, update |Clone| and possibly |ResetForCommit|.
  // WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING

  // Tree of FrameNavigationEntries, one for each frame on the page.
  // FrameNavigationEntries may be shared with other NavigationEntries;
  // TreeNodes are not shared.
  std::unique_ptr<TreeNode> frame_tree_;

  // See the accessors above for descriptions.
  int unique_id_;
  PageType page_type_;
  GURL virtual_url_;
  bool update_virtual_url_with_url_;
  std::u16string title_;
  // The app title is optional and may be empty. If set to a non-empty value, a
  // web app displayed in an app window may use this string instead of the
  // regular title. See
  // https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/DocumentSubtitle/explainer.md
  std::optional<std::u16string> app_title_;
  FaviconStatus favicon_;
  SSLStatus ssl_;
  ui::PageTransition transition_type_;
  GURL user_typed_url_;
  RestoreType restore_type_;
  GURL original_request_url_;
  bool is_overriding_user_agent_;
  base::Time timestamp_;
  int http_status_code_;

  // This member is not persisted with session restore because it is transient.
  // If the post request succeeds, this field is cleared since the same
  // information is stored in PageState. It is also only shallow copied with
  // compiler provided copy constructor.  Cleared in |ResetForCommit|.
  scoped_refptr<network::ResourceRequestBody> post_data_;

  // This member is not persisted with session restore.
  std::string extra_headers_;

  // Used for specifying base URL for pages loaded via data URLs. Only used and
  // persisted by Android WebView.
  GURL base_url_for_data_url_;

#if BUILDFLAG(IS_ANDROID)
  // Used for passing really big data URLs from browser to renderers. Only used
  // and persisted by Android WebView.
  scoped_refptr<const base::RefCountedString> data_url_as_string_;
#endif

  // Whether the entry, while loading, was created for a renderer-initiated
  // navigation.  This dictates whether the URL should be displayed before the
  // navigation commits.  It is cleared in |ResetForCommit| and not persisted.
  bool is_renderer_initiated_;

  // This is a cached version of the result of GetTitleForDisplay. It prevents
  // us from having to do URL formatting on the URL every time the title is
  // displayed. When the URL, virtual URL, or title is set, this should be
  // cleared to force a refresh.
  mutable std::u16string cached_display_title_;

  // This is set to true when this entry's navigation should clear the session
  // history both on the renderer and browser side. The browser side history
  // won't be cleared until the renderer has committed this navigation. This
  // entry is not persisted by the session restore system, as it is always
  // cleared in |ResetForCommit|.
  bool should_clear_history_list_;

  // Set when this entry should be able to access local file:// resources. This
  // value is not needed after the entry commits and is not persisted.
  bool can_load_local_resources_;

  // If valid, this indicates which FrameTreeNode to navigate.  This field is
  // not persisted because it is experimental and only used when the
  // --site-per-process flag is passed.  It is cleared in |ResetForCommit|
  // because we only use it while the navigation is pending.
  // TODO(creis): Move this to FrameNavigationEntry.
  FrameTreeNodeId frame_tree_node_id_;

  // Whether the URL load carries a user gesture.
  bool has_user_gesture_;

  // Used to store ReloadType for the entry.  This is ReloadType::NONE for
  // non-reload navigations.  Reset at commit and not persisted.
  ReloadType reload_type_;

  // Determine if the navigation was started within a context menu.
  bool started_from_context_menu_;

  // Set to true if the navigation controller gets notified about a SSL error
  // for a pending navigation. Defaults to false.
  bool ssl_error_;

  // The net::IsolationInfo for this NavigationEntry. If provided, this
  // determines the IsolationInfo to be used when navigating to this
  // NavigationEntry; otherwise, it is determined based on the navigating frame
  // and top frame origins. For example, this is used for view-source.
  std::optional<net::IsolationInfo> isolation_info_;

  // Stores information about the entry prior to being replaced (e.g.
  // history.replaceState()). It is preserved after commit (session sync for
  // offline analysis) but should not be persisted. The concept is valid for
  // subframe navigations but we only need to track it for main frames, that's
  // why the field is listed here.
  std::optional<ReplacedNavigationEntryData> replaced_entry_data_;

  // Set to true if this page does a navigation without ever receiving a user
  // gesture. If true, it will be skipped on subsequent back/forward button
  // clicks. This is to intervene against pages that manipulate the history such
  // that the user is not able to go back to the last site they interacted with.
  // Navigation here implies both client side redirects and history.pushState
  // calls.
  // It is always false the first time an entry's navigation is committed and
  // is also reset to false if an entry is reused for any subsequent
  // navigations.
  // TODO(shivanisha): Persist this field once the intervention is stable.
  bool should_skip_on_back_forward_ui_;

  // TODO(altimin, crbug.com/933147): Remove this logic after we are done
  // with implement back-forward cache.
  // It is preserved at commit but not persisted.
  scoped_refptr<BackForwardCacheMetrics> back_forward_cache_metrics_;

  // See comment for the enum for explanation.
  InitialNavigationEntryState initial_navigation_entry_state_ =
      InitialNavigationEntryState::kNonInitial;

  // Information about a navigation transition. See the comments on the class
  // for details.
  NavigationTransitionData navigation_transition_data_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_ENTRY_IMPL_H_
