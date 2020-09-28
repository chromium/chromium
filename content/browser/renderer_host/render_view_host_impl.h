// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/process/kill.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/input_device_change_observer.h"
#include "content/browser/renderer_host/page_lifecycle_state_manager.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_owner_delegate.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_view_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_states.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_switching_observer.h"

namespace blink {
namespace web_pref {
struct WebPreferences;
}
}  // namespace blink

namespace content {

class AgentSchedulingGroupHost;
class RenderProcessHost;
class TimeoutMonitor;

// A callback which will be called immediately before EnterBackForwardCache
// starts.
using WillEnterBackForwardCacheCallbackForTesting =
    base::RepeatingCallback<void()>;

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
// (if frame specific) or WebContentsImpl (if page specific).
//
// For context, please see https://crbug.com/467770 and
// https://www.chromium.org/developers/design-documents/site-isolation.
class CONTENT_EXPORT RenderViewHostImpl
    : public RenderViewHost,
      public RenderWidgetHostOwnerDelegate,
      public RenderProcessHostObserver,
      public ui::GpuSwitchingObserver,
      public IPC::Listener,
      public base::RefCounted<RenderViewHostImpl> {
 public:
  // Convenience function, just like RenderViewHost::FromID.
  static RenderViewHostImpl* FromID(int process_id, int routing_id);

  // Convenience function, just like RenderViewHost::From.
  static RenderViewHostImpl* From(RenderWidgetHost* rwh);

  static void GetPlatformSpecificPrefs(
      blink::mojom::RendererPreferences* prefs);

  // Checks whether any RenderViewHostImpl instance associated with a given
  // process is not currently in the back-forward cache.
  // TODO(https://crbug.com/1125996): Remove once a well-behaved frozen
  // RenderFrame never send IPCs messages, even if there are active pages in the
  // process.
  static bool HasNonBackForwardCachedInstancesForProcess(
      RenderProcessHost* process);

  RenderViewHostImpl(SiteInstance* instance,
                     std::unique_ptr<RenderWidgetHostImpl> widget,
                     RenderViewHostDelegate* delegate,
                     int32_t routing_id,
                     int32_t main_frame_routing_id,
                     bool swapped_out,
                     bool has_initialized_audio_host);

  // RenderViewHost implementation.
  bool Send(IPC::Message* msg) override;
  RenderWidgetHostImpl* GetWidget() override;
  RenderProcessHost* GetProcess() override;
  int GetRoutingID() override;
  RenderFrameHost* GetMainFrame() override;
  void EnablePreferredSizeMode() override;
  void ExecutePluginActionAtLocation(
      const gfx::Point& location,
      blink::mojom::PluginActionType action) override;
  RenderViewHostDelegate* GetDelegate() override;
  SiteInstanceImpl* GetSiteInstance() override;
  bool IsRenderViewLive() override;
  void NotifyMoveOrResizeStarted() override;

  void SendWebPreferencesToRenderer();

  // RenderProcessHostObserver implementation
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // GpuSwitchingObserver implementation.
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override;

  // Set up the RenderView child process. Virtual because it is overridden by
  // TestRenderViewHost.
  // |opener_route_id| parameter indicates which RenderView created this
  //   (MSG_ROUTING_NONE if none).
  // |window_was_created_with_opener| is true if this top-level frame was
  //   created with an opener. (The opener may have been closed since.)
  // |proxy_route_id| is only used when creating a RenderView in an inactive
  //   state.
  virtual bool CreateRenderView(
      const base::Optional<base::UnguessableToken>& opener_frame_token,
      int proxy_route_id,
      bool window_was_created_with_opener);

  // Tracks whether this RenderViewHost is in an active state (rather than
  // pending unload or unloaded), according to its main frame
  // RenderFrameHost.
  bool is_active() const { return main_frame_routing_id_ != MSG_ROUTING_NONE; }

  // TODO(creis): Remove as part of http://crbug.com/418265.
  bool is_waiting_for_page_close_completion() const {
    return is_waiting_for_page_close_completion_;
  }

  // Generate RenderViewCreated events for observers through the delegate.
  // These events are only generated for active RenderViewHosts (which have a
  // RenderFrameHost for the main frame) as well as inactive RenderViewHosts
  // that have a pending main frame navigation; i.e., this is done only when
  // GetMainFrame() is non-null.
  //
  // This function also ensures that a particular RenderViewHost never
  // dispatches these events more than once.  For example, if a RenderViewHost
  // transitions from active to inactive after a cross-process navigation
  // (where it no longer has a main frame RenderFrameHost), and then back to
  // active after another cross-process navigation, this function will filter
  // out the second notification.
  //
  // TODO(alexmos): Deprecate RenderViewCreated and remove this.  See
  // https://crbug.com/763548.
  void DispatchRenderViewCreated();

  // Returns the `AgentSchedulingGroupHost` this view is associated with (via
  // the widget).
  AgentSchedulingGroupHost& GetAgentSchedulingGroup();

  // Tells the renderer process to request a page-scale animation based on the
  // specified point/rect.
  void AnimateDoubleTapZoom(const gfx::Point& point, const gfx::Rect& rect);

  // Tells the renderer process to run the page's unload handler.
  // A completion callback is invoked by the renderer when the handler
  // execution completes.
  void ClosePage();

  // Close the page ignoring whether it has unload events registers.
  // This is called after the beforeunload and unload events have fired
  // and the user has agreed to continue with closing the page.
  void ClosePageIgnoringUnloadEvents();

  // Requests a page-scale animation based on the specified rect.
  void ZoomToFindInPageRect(const gfx::Rect& rect_to_zoom);

  // Tells the renderer view to focus the first (last if reverse is true) node.
  void SetInitialFocus(bool reverse);

  bool SuddenTerminationAllowed();
  void set_sudden_termination_allowed(bool enabled) {
    sudden_termination_allowed_ = enabled;
  }

  // Send RenderViewReady to observers once the process is launched, but not
  // re-entrantly.
  void PostRenderViewReady();

  // Passes current web preferences to the renderer after recomputing all of
  // them, including the slow-to-compute hardware preferences.
  // (WebContents::OnWebPreferencesChanged is a faster alternate that avoids
  // slow recomputations.)
  void OnHardwareConfigurationChanged();

  // Sets the routing id for the main frame. When set to MSG_ROUTING_NONE, the
  // view is not considered active.
  void SetMainFrameRoutingId(int routing_id);

  // Called when the RenderFrameHostImpls/RenderFrameProxyHosts that own this
  // RenderViewHost enter the BackForwardCache.
  void EnterBackForwardCache();

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

  void SetVisibility(blink::mojom::PageVisibilityState visibility);

  void SetIsFrozen(bool frozen);
  void OnBackForwardCacheTimeout();

  PageLifecycleStateManager* GetPageLifecycleStateManager() {
    return page_lifecycle_state_manager_.get();
  }

  // Called during frame eviction to return all SurfaceIds in the frame tree.
  // Marks all views in the frame tree as evicted.
  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction();

  // Resets any per page state. This should be called when a main frame
  // associated with this RVH commits a navigation to a new document. Note that
  // this means it should NOT be called for same document navigations or when
  // restoring a page from the back-forward cache.
  void ResetPerPageState();

  bool did_first_visually_non_empty_paint() const {
    return did_first_visually_non_empty_paint_;
  }

  void OnThemeColorChanged(RenderFrameHostImpl* rfh,
                           const base::Optional<SkColor>& theme_color);

  void DidChangeBackgroundColor(RenderFrameHostImpl* rfh,
                                const SkColor& background_color);

  base::Optional<SkColor> theme_color() const {
    return main_frame_theme_color_;
  }

  base::Optional<SkColor> background_color() const {
    return main_frame_background_color_;
  }

  void SetContentsMimeType(std::string mime_type);
  const std::string& contents_mime_type() { return contents_mime_type_; }

  // Notifies that / returns whether main document's onload() handler was
  // completed.
  void DocumentOnLoadCompletedInMainFrame();
  bool IsDocumentOnLoadCompletedInMainFrame();

  // Manual RTTI to ensure safe downcasts in tests.
  virtual bool IsTestRenderViewHost() const;

  void SetWillEnterBackForwardCacheCallbackForTesting(
      const WillEnterBackForwardCacheCallbackForTesting& callback);

  void BindPageBroadcast(
      mojo::PendingAssociatedRemote<blink::mojom::PageBroadcast>
          page_broadcast);

  // The remote mojom::PageBroadcast interface that is used to send messages to
  // the renderer's blink::WebViewImpl when broadcasting messages to all
  // renderers hosting frames in the frame tree.
  const mojo::AssociatedRemote<blink::mojom::PageBroadcast>&
  GetAssociatedPageBroadcast();

  // NOTE: Do not add functions that just send an IPC message that are called in
  // one or two places. Have the caller send the IPC message directly (unless
  // the caller places are in different platforms, in which case it's better
  // to keep them consistent).

 protected:
  friend class RefCounted<RenderViewHostImpl>;
  ~RenderViewHostImpl() override;

  // RenderWidgetHostOwnerDelegate overrides.
  void RenderWidgetDidInit() override;
  void RenderWidgetDidClose() override;
  void RenderWidgetDidFirstVisuallyNonEmptyPaint() override;
  void RenderWidgetGotFocus() override;
  void RenderWidgetLostFocus() override;
  void RenderWidgetDidForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) override;
  bool MayRenderWidgetForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) override;
  bool ShouldContributePriorityToProcess() override;
  void RequestSetBounds(const gfx::Rect& bounds) override;
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
  void OnShowFullscreenWidget(int widget_route_id);
  void OnDidContentsPreferredSizeChange(const gfx::Size& new_size);
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
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CloseWithPendingWhileUnresponsive);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  void RenderViewReady();

  // Called by |close_timeout_| when the page closing timeout fires.
  void ClosePageTimeout();

  void OnPageClosed();

  // TODO(creis): Move to a private namespace on RenderFrameHostImpl.
  // Delay to wait on closing the WebContents for a beforeunload/unload handler
  // to fire.
  static const int64_t kUnloadTimeoutMS;

  // The RenderWidgetHost.
  const std::unique_ptr<RenderWidgetHostImpl> render_widget_host_;

  // Our delegate, which wants to know about changes in the RenderView.
  RenderViewHostDelegate* delegate_;

  // The SiteInstance associated with this RenderViewHost.  All pages drawn
  // in this RenderViewHost are part of this SiteInstance.  Cannot change
  // over time.
  scoped_refptr<SiteInstanceImpl> instance_;

  // Routing ID for this RenderViewHost.
  const int routing_id_;

  // Routing ID for the main frame's RenderFrameHost.
  int main_frame_routing_id_;

  // Set to true when waiting for a blink.mojom.LocalMainFrame.ClosePage()
  // to complete.
  //
  // TODO(creis): Move to RenderFrameHost and RenderWidgetHost.
  // See http://crbug.com/418265.
  bool is_waiting_for_page_close_completion_ = false;

  // True if the render view can be shut down suddenly.
  bool sudden_termination_allowed_ = false;

  // The timeout monitor that runs from when the page close is started in
  // ClosePage() until either the render process ACKs the close with an IPC to
  // OnClosePageACK(), or until the timeout triggers and the page is forcibly
  // closed.
  std::unique_ptr<TimeoutMonitor> close_timeout_;

  // This monitors input changes so they can be reflected to the interaction MQ.
  std::unique_ptr<InputDeviceChangeObserver> input_device_change_observer_;

  // This controls the lifecycle change and notify the renderer.
  std::unique_ptr<PageLifecycleStateManager> page_lifecycle_state_manager_;

  bool updating_web_preferences_ = false;

  // This tracks whether this RenderViewHost has notified observers about its
  // creation with RenderViewCreated.  RenderViewHosts may transition from
  // active (with a RenderFrameHost for the main frame) to inactive state and
  // then back to active, and for the latter transition, this avoids firing
  // duplicate RenderViewCreated events.
  bool has_notified_about_creation_ = false;

  // ---------- Per page state START ------------------------------------------
  // The following members will get reset when this RVH commits a navigation to
  // a new document. See ResetPerPageState()

  // Whether the first visually non-empty paint has occurred.
  bool did_first_visually_non_empty_paint_ = false;

  // The theme color for the underlying document as specified
  // by theme-color meta tag.
  base::Optional<SkColor> main_frame_theme_color_;

  // The background color for the underlying document as computed by CSS.
  base::Optional<SkColor> main_frame_background_color_;

  // Contents MIME type for the main document. It can be used to check whether
  // we can do something for special contents.
  std::string contents_mime_type_;

  // ---------- Per page state END --------------------------------------------

  // BackForwardCache:
  bool is_in_back_forward_cache_ = false;

  // True if the current main document finished executing onload() handler.
  bool is_document_on_load_completed_in_main_frame_ = false;

  WillEnterBackForwardCacheCallbackForTesting
      will_enter_back_forward_cache_callback_for_testing_;

  mojo::AssociatedRemote<blink::mojom::PageBroadcast> page_broadcast_;

  base::WeakPtrFactory<RenderViewHostImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
