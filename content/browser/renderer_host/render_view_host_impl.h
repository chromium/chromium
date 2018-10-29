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
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/input_device_change_observer.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_owner_delegate.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_view_host.h"
#include "net/base/load_states.h"
#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/mojo/window_open_disposition.mojom.h"

namespace content {

struct FrameReplicationState;
class TimeoutMonitor;

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
class CONTENT_EXPORT RenderViewHostImpl : public RenderViewHost,
                                          public RenderWidgetHostOwnerDelegate,
                                          public RenderProcessHostObserver,
                                          public IPC::Listener {
 public:
  // Convenience function, just like RenderViewHost::FromID.
  static RenderViewHostImpl* FromID(int process_id, int routing_id);

  // Convenience function, just like RenderViewHost::From.
  static RenderViewHostImpl* From(RenderWidgetHost* rwh);

  RenderViewHostImpl(SiteInstance* instance,
                     std::unique_ptr<RenderWidgetHostImpl> widget,
                     RenderViewHostDelegate* delegate,
                     int32_t routing_id,
                     int32_t main_frame_routing_id,
                     bool swapped_out,
                     bool has_initialized_audio_host);
  // TODO(ajwong): Make destructor private. Deletion of this object should only
  // be done via ShutdownAndDestroy(). https://crbug.com/545684
  ~RenderViewHostImpl() override;

  // Shuts down this RenderViewHost and deletes it.
  void ShutdownAndDestroy();

  // RenderViewHost implementation.
  bool Send(IPC::Message* msg) override;
  RenderWidgetHostImpl* GetWidget() const override;
  RenderProcessHost* GetProcess() const override;
  int GetRoutingID() const override;
  RenderFrameHost* GetMainFrame() override;
  void DirectoryEnumerationFinished(
      int request_id,
      const std::vector<base::FilePath>& files) override;
  void EnablePreferredSizeMode() override;
  void ExecutePluginActionAtLocation(
      const gfx::Point& location,
      const blink::WebPluginAction& action) override;
  RenderViewHostDelegate* GetDelegate() const override;
  SiteInstanceImpl* GetSiteInstance() const override;
  bool IsRenderViewLive() const override;
  void NotifyMoveOrResizeStarted() override;
  void SetWebUIProperty(const std::string& name,
                        const std::string& value) override;
  void SyncRendererPrefs() override;
  WebPreferences GetWebkitPreferences() override;
  void UpdateWebkitPreferences(const WebPreferences& prefs) override;
  void OnWebkitPreferencesChanged() override;

  // RenderProcessHostObserver implementation
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // Set up the RenderView child process. Virtual because it is overridden by
  // TestRenderViewHost.
  // The |opener_route_id| parameter indicates which RenderView created this
  // (MSG_ROUTING_NONE if none).
  // |window_was_created_with_opener| is true if this top-level frame was
  // created with an opener. (The opener may have been closed since.)
  // The |proxy_route_id| is only used when creating a RenderView in swapped out
  // state.
  // |devtools_frame_token| contains the devtools token for tagging requests and
  // attributing them to the context frame.
  // |replicated_frame_state| contains replicated data for the top-level frame,
  // such as its name and sandbox flags.
  virtual bool CreateRenderView(
      int opener_frame_route_id,
      int proxy_route_id,
      const base::UnguessableToken& devtools_frame_token,
      const FrameReplicationState& replicated_frame_state,
      bool window_was_created_with_opener);

  base::TerminationStatus render_view_termination_status() const {
    return render_view_termination_status_;
  }

  // Tracks whether this RenderViewHost is in an active state (rather than
  // pending swap out or swapped out), according to its main frame
  // RenderFrameHost.
  bool is_active() const { return is_active_; }
  void SetIsActive(bool is_active);

  // Tracks whether this RenderViewHost is swapped out, according to its main
  // frame RenderFrameHost.
  void set_is_swapped_out(bool is_swapped_out) {
    is_swapped_out_ = is_swapped_out;
  }

  // TODO(creis): Remove as part of http://crbug.com/418265.
  bool is_waiting_for_close_ack() const { return is_waiting_for_close_ack_; }

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

  // Tells the renderer process to run the page's unload handler.
  // A ClosePage_ACK ack is sent back when the handler execution completes.
  void ClosePage();

  // Close the page ignoring whether it has unload events registers.
  // This is called after the beforeunload and unload events have fired
  // and the user has agreed to continue with closing the page.
  void ClosePageIgnoringUnloadEvents();

  // Tells the renderer view to focus the first (last if reverse is true) node.
  void SetInitialFocus(bool reverse);

  bool SuddenTerminationAllowed() const;
  void set_sudden_termination_allowed(bool enabled) {
    sudden_termination_allowed_ = enabled;
  }

  // Creates a new RenderWidget with the given route id.
  void CreateNewWidget(int32_t route_id, mojom::WidgetPtr widget);

  // Creates a full screen RenderWidget.
  void CreateNewFullscreenWidget(int32_t route_id, mojom::WidgetPtr widget);

  // Send RenderViewReady to observers once the process is launched, but not
  // re-entrantly.
  void PostRenderViewReady();

  void set_main_frame_routing_id(int routing_id) {
    main_frame_routing_id_ = routing_id;
  }

  // Increases the refcounting on this RVH. This is done by the FrameTree on
  // creation of a RenderFrameHost or RenderFrameProxyHost.
  void increment_ref_count() { ++frames_ref_count_; }

  // Decreases the refcounting on this RVH. This is done by the FrameTree on
  // destruction of a RenderFrameHost or RenderFrameProxyHost.
  void decrement_ref_count() { --frames_ref_count_; }

  // Returns the refcount on this RVH, that is the number of RenderFrameHosts
  // and RenderFrameProxyHosts currently using it.
  int ref_count() { return frames_ref_count_; }

  // NOTE: Do not add functions that just send an IPC message that are called in
  // one or two places. Have the caller send the IPC message directly (unless
  // the caller places are in different platforms, in which case it's better
  // to keep them consistent).

 protected:
  // RenderWidgetHostOwnerDelegate overrides.
  void RenderWidgetDidInit() override;
  void RenderWidgetDidClose() override;
  void RenderWidgetNeedsToRouteCloseEvent() override;
  void RenderWidgetWillSetIsLoading(bool is_loading) override;
  void RenderWidgetDidFirstVisuallyNonEmptyPaint() override;
  void RenderWidgetDidCommitAndDrawCompositorFrame() override;
  void RenderWidgetGotFocus() override;
  void RenderWidgetLostFocus() override;
  void RenderWidgetDidForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) override;
  bool MayRenderWidgetForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) override;
  bool ShouldContributePriorityToProcess() override;
  void RequestSetBounds(const gfx::Rect& bounds) override;

  // IPC message handlers.
  void OnShowView(int route_id,
                  WindowOpenDisposition disposition,
                  const gfx::Rect& initial_rect,
                  bool user_gesture);
  void OnShowWidget(int widget_route_id, const gfx::Rect& initial_rect);
  void OnShowFullscreenWidget(int widget_route_id);
  void OnUpdateTargetURL(const GURL& url);
  void OnDocumentAvailableInMainFrame(bool uses_temporary_zoom_level);
  void OnDidContentsPreferredSizeChange(const gfx::Size& new_size);
  void OnPasteFromSelectionClipboard();
  void OnTakeFocus(bool reverse);
  void OnClosePageACK();
  void OnDidZoomURL(double zoom_level, const GURL& url);
  void OnFocus();

 private:
  // TODO(nasko): Temporarily friend RenderFrameHostImpl, so we don't duplicate
  // utility functions and state needed in both classes, while we move frame
  // specific code away from this class.
  friend class RenderFrameHostImpl;
  friend class TestRenderViewHost;
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, BasicRenderFrameHost);
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, RoutingIdSane);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CleanUpSwappedOutRVHOnProcessCrash);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CloseWithPendingWhileUnresponsive);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           NavigateMainFrameToChildSite);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  void RenderViewReady();

  // Called by |close_timeout_| when the page closing timeout fires.
  void ClosePageTimeout();

  // TODO(creis): Move to a private namespace on RenderFrameHostImpl.
  // Delay to wait on closing the WebContents for a beforeunload/unload handler
  // to fire.
  static const int64_t kUnloadTimeoutMS;

  // Returns the content specific prefs for this RenderViewHost.
  // TODO(creis): Move most of this method to RenderProcessHost, since it's
  // mostly the same across all RVHs in a process.  Move the rest to RFH.
  // See https://crbug.com/304341.
  WebPreferences ComputeWebkitPrefs();

  // The RenderWidgetHost.
  std::unique_ptr<RenderWidgetHostImpl> render_widget_host_;

  // The number of RenderFrameHosts which have a reference to this RVH.
  int frames_ref_count_;

  // Our delegate, which wants to know about changes in the RenderView.
  RenderViewHostDelegate* delegate_;

  // The SiteInstance associated with this RenderViewHost.  All pages drawn
  // in this RenderViewHost are part of this SiteInstance.  Cannot change
  // over time.
  scoped_refptr<SiteInstanceImpl> instance_;

  // Tracks whether this RenderViewHost is in an active state.  False if the
  // main frame is pending swap out, pending deletion, or swapped out, because
  // it is not visible to the user in any of these cases.
  bool is_active_;

  // Tracks whether the main frame RenderFrameHost is swapped out.  Unlike
  // is_active_, this is false when the frame is pending swap out or deletion.
  // TODO(creis): Remove this when we no longer filter IPCs after swap out.
  // See https://crbug.com/745091.
  bool is_swapped_out_;

  // Routing ID for this RenderViewHost.
  const int routing_id_;

  // Routing ID for the main frame's RenderFrameHost.
  int main_frame_routing_id_;

  // Set to true when waiting for a ViewHostMsg_ClosePageACK.
  // TODO(creis): Move to RenderFrameHost and RenderWidgetHost.
  // See http://crbug.com/418265.
  bool is_waiting_for_close_ack_;

  // True if the render view can be shut down suddenly.
  bool sudden_termination_allowed_;

  // The termination status of the last render view that terminated.
  base::TerminationStatus render_view_termination_status_;

  // This is updated every time UpdateWebkitPreferences is called. That method
  // is in turn called when any of the settings change that the WebPreferences
  // values depend on.
  std::unique_ptr<WebPreferences> web_preferences_;

  // The timeout monitor that runs from when the page close is started in
  // ClosePage() until either the render process ACKs the close with an IPC to
  // OnClosePageACK(), or until the timeout triggers and the page is forcibly
  // closed.
  std::unique_ptr<TimeoutMonitor> close_timeout_;

  // This monitors input changes so they can be reflected to the interaction MQ.
  std::unique_ptr<InputDeviceChangeObserver> input_device_change_observer_;

  bool updating_web_preferences_;

  // This tracks whether this RenderViewHost has notified observers about its
  // creation with RenderViewCreated.  RenderViewHosts may transition from
  // active (with a RenderFrameHost for the main frame) to inactive state and
  // then back to active, and for the latter transition, this avoids firing
  // duplicate RenderViewCreated events.
  bool has_notified_about_creation_;

  base::WeakPtrFactory<RenderViewHostImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
