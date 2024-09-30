// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/safe_ref.h"
#include "base/process/kill.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/browsing_context_state.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/page_lifecycle_state_manager.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_owner_delegate.h"
#include "content/browser/site_instance_group.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_export.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_view_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_states.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"

namespace blink {
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace content {

class AgentSchedulingGroupHost;
class RenderProcessHost;

// A callback which will be called immediately before EnterBackForwardCache
// starts.
using WillEnterBackForwardCacheCallbackForTesting =
    base::RepeatingCallback<void()>;

// A callback which will be called immediately before sending the
// RendererPreferences information to the renderer.
using WillSendRendererPreferencesCallbackForTesting =
    base::RepeatingCallback<void(const blink::RendererPreferences&)>;

// A callback which will be called immediately before sending the WebPreferences
// information to the renderer.
using WillSendWebPreferencesCallbackForTesting = base::RepeatingClosure;

// This implements the RenderViewHost interface that is exposed to
// embedders of content, and adds things only visible to content.
//
// The exact API of this object needs to be more thoroughly designed. Right
// now it mimics what WebContentsImpl exposed, which is a fairly large API and
// may contain things that are not relevant to a common subset of views. See
// also the comment in render_view_host_delegate.h about the size and scope of
// the delegate API.
//
// Right now, the concept of page navigation (both top level and frame) exists
// in the WebContentsImpl still, so if you instantiate one of these elsewhere,
// you will not be able to traverse pages back and forward. We need to determine
// if we want to bring that and other functionality down into this object so it
// can be shared by others.
//
// DEPRECATED: RenderViewHostImpl is being removed as part of the SiteIsolation
// project. New code should not be added here, but to either RenderFrameHostImpl
// (if frame specific) or PageImpl (if page specific).
//
// For context, please see https://crbug.com/467770 and
// https://www.chromium.org/developers/design-documents/site-isolation.
class CONTENT_EXPORT RenderViewHostImpl
    : public RenderViewHost,
      public RenderWidgetHostOwnerDelegate,
      public RenderProcessHostObserver,
      public IPC::Listener,
      public base::RefCounted<RenderViewHostImpl> {
 public:
  // Convenience function, just like RenderViewHost::FromID.
  static RenderViewHostImpl* FromID(int process_id, int routing_id);

  // Convenience function, just like RenderViewHost::From.
  static RenderViewHostImpl* From(RenderWidgetHost* rwh);

  static void GetPlatformSpecificPrefs(blink::RendererPreferences* prefs);

  // Checks whether any RenderViewHostImpl instance associated with a given
  // process is not currently in the back-forward cache.
  // TODO(crbug.com/40147948): Remove once a well-behaved frozen
  // RenderFrame never send IPCs messages, even if there are active pages in the
  // process.
  static bool HasNonBackForwardCachedInstancesForProcess(
      RenderProcessHost* process);

  RenderViewHostImpl(
      FrameTree* frame_tree,
      SiteInstanceGroup* group,
      const StoragePartitionConfig& storage_partition_config,
      std::unique_ptr<RenderWidgetHostImpl> widget,
      RenderViewHostDelegate* delegate,
      int32_t routing_id,
      int32_t main_frame_routing_id,
      bool has_initialized_audio_host,
      scoped_refptr<BrowsingContextState> main_browsing_context_state,
      CreateRenderViewHostCase create_case);

  RenderViewHostImpl(const RenderViewHostImpl&) = delete;
  RenderViewHostImpl& operator=(const RenderViewHostImpl&) = delete;

  // RenderViewHost implementation.
  RenderWidgetHostImpl* GetWidget() const override;
  RenderProcessHost* GetProcess() const override;
  int GetRoutingID() const override;
  void EnablePreferredSizeMode() override;
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> context) const override;

  void SendWebPreferencesToRenderer();
  void SendRendererPreferencesToRenderer(
      const blink::RendererPreferences& preferences);

  // RenderProcessHostObserver implementation
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // Set up the `blink::WebView` child process. Virtual because it is overridden
  // by TestRenderViewHost.
  // `opener_route_id` parameter indicates which `blink::WebView` created this
  //   (MSG_ROUTING_NONE if none).
  // `window_was_opened_by_another_window` is true if this top-level frame was
  //   created by another window, as opposed to independently created (through
  //   the browser UI, etc). This is true even when the window is opened with
  //   "noopener", and even if the opener has been closed since.
  // `proxy_route_id` is only used when creating a `blink::WebView` in an
  //   inactive state.
  virtual bool CreateRenderView(
      const std::optional<blink::FrameToken>& opener_frame_token,
      int proxy_route_id,
      bool window_was_opened_by_another_window);

  RenderViewHostDelegate* GetDelegate();

  bool is_speculative() { return is_speculative_; }
  void set_is_speculative(bool is_speculative) {
    is_speculative_ = is_speculative;
  }

  bool is_registered_with_frame_tree() { return registered_with_frame_tree_; }
  void set_is_registered_with_frame_tree(bool is_registered) {
    registered_with_frame_tree_ = is_registered;
  }

  bool renderer_view_created() const { return renderer_view_created_; }

  FrameTree::RenderViewHostMapId rvh_map_id() const {
    return render_view_host_map_id_;
  }

  base::WeakPtr<RenderViewHostImpl> GetWeakPtr();

  // Tracks whether this RenderViewHost is in an active state (rather than
  // pending unload or unloaded), according to its main frame
  // RenderFrameHost.
  bool is_active() const { return main_frame_routing_id_ != MSG_ROUTING_NONE; }
  int main_frame_routing_id() const { return main_frame_routing_id_; }

  // Returns true if the `blink::WebView` is active and has not crashed.
  bool IsRenderViewLive() const;

  // Called when the `blink::WebView` in the renderer process has been created,
  // at which point IsRenderViewLive() becomes true, and the mojo connections to
  // the renderer process for this view now exist.
  void RenderViewCreated(RenderFrameHostImpl* local_main_frame);

  // Returns the main RenderFrameHostImpl associated with this RenderViewHost or
  // null if it doesn't exist. It's null if the main frame is represented in
  // this RenderViewHost by RenderFrameProxyHost (from Blink perspective,
  // blink::Page's main blink::Frame is remote).
  RenderFrameHostImpl* GetMainRenderFrameHost();

  // RenderViewHost is associated with a given SiteInstanceGroup and as
  // BrowsingContextState in non-legacy BrowsingContextState mode is tied to a
  // given BrowsingInstance, so the main BrowsingContextState stays the same
  // during the entire lifetime of a RenderViewHost: cross-SiteInstanceGroup
  // same-BrowsingInstance navigations might change the representation of the
  // main frame in a given `blink::WebView` from RenderFrame to
  // `blink::RemoteFrame` and back, while cross-BrowsingInstances result in
  // creating a new unrelated RenderViewHost. This is not true in the legacy BCS
  // mode, so there the `main_browsing_context_state_` is null.
  const std::optional<base::SafeRef<BrowsingContextState>>&
  main_browsing_context_state() const {
    return main_browsing_context_state_;
  }

  // Returns the `AgentSchedulingGroupHost` this view is associated with (via
  // the widget).
  AgentSchedulingGroupHost& GetAgentSchedulingGroup() const;

  // Tells the renderer process to request a page-scale animation based on the
  // specified point/rect.
  void AnimateDoubleTapZoom(const gfx::Point& point, const gfx::Rect& rect);

  // Requests a page-scale animation based on the specified rect.
  void ZoomToFindInPageRect(const gfx::Rect& rect_to_zoom);

  // Tells the renderer view to focus the first (last if reverse is true) node.
  void SetInitialFocus(bool reverse);

  // Send RenderViewReady to observers once the process is launched, but not
  // re-entrantly.
  void PostRenderViewReady();

  // Sets the routing id for the main frame. When set to MSG_ROUTING_NONE, the
  // view is not considered active.
  void SetMainFrameRoutingId(int routing_id);

  // Called when the RenderFrameHostImpls/RenderFrameProxyHosts that own this
  // RenderViewHost enter the BackForwardCache.
  void EnterBackForwardCache();

  // Indicates whether or not |this| has received an acknowledgement from
  // renderer that it has enered BackForwardCache.
  bool DidReceiveBackForwardCacheAck();

  // Called when the RenderFrameHostImpls/RenderFrameProxyHosts that own this
  // RenderViewHost leave the BackForwardCache. This occurs immediately before a
  // restored document is committed.
  // |page_restore_params| includes information that is needed by the page after
  // getting restored, which includes the latest history information (offset,
  // length) and the timestamp corresponding to the start of the back-forward
  // cached navigation, which would be communicated to the page to allow it to
  // record the latency of this navigation.
  void LeaveBackForwardCache(
      blink::mojom::PageRestoreParamsPtr page_restore_params);

  bool is_in_back_forward_cache() const { return is_in_back_forward_cache_; }

  void ActivatePrerenderedPage(blink::mojom::PrerenderPageActivationParamsPtr
                                   prerender_page_activation_params,
                               base::OnceClosure callback);

  void SetFrameTreeVisibility(blink::mojom::PageVisibilityState visibility);

  void SetIsFrozen(bool frozen);
  void OnBackForwardCacheTimeout();
  void MaybeEvictFromBackForwardCache();
  void EnforceBackForwardCacheSizeLimit();

  PageLifecycleStateManager* GetPageLifecycleStateManager() {
    return page_lifecycle_state_manager_.get();
  }

  // Called during frame eviction to return all SurfaceIds in the frame tree.
  // Marks all views in the frame tree as evicted.
  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction();

  // Manual RTTI to ensure safe downcasts in tests.
  virtual bool IsTestRenderViewHost() const;

  void SetWillEnterBackForwardCacheCallbackForTesting(
      const WillEnterBackForwardCacheCallbackForTesting& callback);

  void SetWillSendRendererPreferencesCallbackForTesting(
      const WillSendRendererPreferencesCallbackForTesting& callback);

  void SetWillSendWebPreferencesCallbackForTesting(
      const WillSendWebPreferencesCallbackForTesting& callback);

  void BindPageBroadcast(
      mojo::PendingAssociatedRemote<blink::mojom::PageBroadcast>
          page_broadcast);

  // The remote mojom::PageBroadcast interface that is used to send messages to
  // the renderer's blink::WebViewImpl when broadcasting messages to all
  // renderers hosting frames in the frame tree.
  const mojo::AssociatedRemote<blink::mojom::PageBroadcast>&
  GetAssociatedPageBroadcast();

  // Prepares the renderer page to leave the back-forward cache by disabling
  // Javascript eviction. |done_cb| is called upon receipt of the
  // acknowledgement from the renderer that this has actually happened.
  //
  // After |done_cb| is called you can be certain that this renderer will not
  // trigger an eviction of this page.
  void PrepareToLeaveBackForwardCache(base::OnceClosure done_cb);

  // TODO(crbug.com/40169570): FrameTree and FrameTreeNode will not be
  // const as with prerenderer activation the page needs to move between
  // FrameTreeNodes and FrameTrees. As it's hard to make sure that all places
  // handle this transition correctly, MPArch will remove references from this
  // class to FrameTree/FrameTreeNode.
  FrameTree* frame_tree() const { return frame_tree_; }
  void SetFrameTree(FrameTree& frame_tree);

  // Mark this RenderViewHost as not available for reuse. This will remove
  // it from being registered with the associated FrameTree.
  void DisallowReuse();

  base::SafeRef<RenderViewHostImpl> GetSafeRef();

  mojom::ViewWidgetType ViewWidgetType();

  SiteInstanceGroup* site_instance_group() const {
    return &*site_instance_group_;
  }

  // NOTE: Do not add functions that just send an IPC message that are called in
  // one or two places. Have the caller send the IPC message directly (unless
  // the caller places are in different platforms, in which case it's better
  // to keep them consistent).

 protected:
  friend class base::RefCounted<RenderViewHostImpl>;
  ~RenderViewHostImpl() override;

  // RenderWidgetHostOwnerDelegate overrides.
  void RenderWidgetGotFocus() override;
  void RenderWidgetLostFocus() override;
  void RenderWidgetDidForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) override;
  bool MayRenderWidgetForwardKeyboardEvent(
      const input::NativeWebKeyboardEvent& key_event) override;
  bool ShouldContributePriorityToProcess() override;
  void SetBackgroundOpaque(bool opaque) override;
  bool IsMainFrameActive() override;
  bool IsNeverComposited() override;
  blink::web_pref::WebPreferences GetWebkitPreferencesForWidget() override;

  // IPC message handlers.
  void OnShowView(int route_id,
                  WindowOpenDisposition disposition,
                  const gfx::Rect& initial_rect,
                  bool user_gesture);
  void OnShowWidget(int widget_route_id, const gfx::Rect& initial_rect);
  void OnPasteFromSelectionClipboard();
  void OnTakeFocus(bool reverse);
  void OnFocus();

 private:
  // TODO(nasko): Temporarily friend RenderFrameHostImpl, so we don't duplicate
  // utility functions and state needed in both classes, while we move frame
  // specific code away from this class.
  friend class RenderFrameHostImpl;
  friend class TestRenderViewHost;
  friend class PageLifecycleStateManagerBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, BasicRenderFrameHost);
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, RoutingIdSane);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  std::string ToDebugString() override;

  void RenderViewReady();

  // The RenderWidgetHost.
  const std::unique_ptr<RenderWidgetHostImpl> render_widget_host_;

  // Our delegate, which wants to know about changes in the `blink::WebView`.
  raw_ptr<RenderViewHostDelegate> delegate_;

  // ID to use when registering/unregistering this object with its FrameTree.
  // This ID is generated by passing a SiteInstanceGroup to
  // FrameTree::GetRenderViewHostMapId(). This RenderViewHost may only be reused
  // by frames with SiteInstanceGroups that generate an ID that matches this
  // field.
  FrameTree::RenderViewHostMapId render_view_host_map_id_;

  // The SiteInstanceGroup this RenderViewHostImpl belongs to.
  // TODO(crbug.com/40258727) Turn this into base::SafeRef
  base::WeakPtr<SiteInstanceGroup> site_instance_group_;

  // Provides information for selecting the session storage namespace for this
  // view.
  const StoragePartitionConfig storage_partition_config_;

  // Routing ID for this RenderViewHost.
  const int routing_id_;

  // Whether the renderer-side `blink::WebView` is created. Becomes false when
  // the renderer crashes.
  bool renderer_view_created_ = false;

  // Routing ID for the main frame's RenderFrameHost.
  int main_frame_routing_id_;

  std::optional<mojom::ViewWidgetType> view_widget_type_;

  // This controls the lifecycle change and notify the renderer.
  std::unique_ptr<PageLifecycleStateManager> page_lifecycle_state_manager_;

  bool updating_web_preferences_ = false;

  // BackForwardCache:
  bool is_in_back_forward_cache_ = false;

  WillEnterBackForwardCacheCallbackForTesting
      will_enter_back_forward_cache_callback_for_testing_;

  WillSendRendererPreferencesCallbackForTesting
      will_send_renderer_preferences_callback_for_testing_;

  WillSendWebPreferencesCallbackForTesting
      will_send_web_preferences_callback_for_testing_;

  mojo::AssociatedRemote<blink::mojom::PageBroadcast> page_broadcast_;

  raw_ptr<FrameTree> frame_tree_;

  // See main_browsing_context_state() for more details.
  std::optional<base::SafeRef<BrowsingContextState>>
      main_browsing_context_state_;

  bool registered_with_frame_tree_ = false;

  // Whether the RenderViewHost is a speculative RenderViewHost or not.
  // Currently this is never set, as the feature is not implemented yet.
  // TODO(crbug.com/40228869): Actually set this value for speculative
  // RenderViewHosts.
  bool is_speculative_ = false;

  base::WeakPtrFactory<RenderViewHostImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
