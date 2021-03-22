// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
#define CONTENT_RENDERER_RENDER_VIEW_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/id_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/render_frame_impl.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/surface/transport_dib.h"

namespace blink {
class WebURLRequest;
struct WebWindowFeatures;
}  // namespace blink

namespace content {
class AgentSchedulingGroup;
class RenderViewImplTest;
class RenderViewObserver;
class RenderViewTest;

namespace mojom {
class CreateViewParams;
}

// RenderViewImpl (the implementation of RenderView) is the renderer process
// object that owns the blink frame tree.
//
// Each top-level web container has a frame tree, and thus a RenderViewImpl.
// Typically such a container is a browser tab, or a tab-less window. It can
// also be other cases such as a background page or extension.
//
// Under site isolation, frames in the main frame's tree may be moved out
// to a separate frame tree (possibly in another process), leaving remote
// placeholders behind. Each such frame tree also includes a RenderViewImpl as
// the owner of it. Thus a tab may have multiple RenderViewImpls, one for the
// main frame, and one for each other frame tree generated.
class CONTENT_EXPORT RenderViewImpl : public blink::WebViewClient,
                                      public IPC::Listener,
                                      public RenderView {
 public:
  // Creates a new RenderView. Note that if the original opener has been closed,
  // |params.window_was_created_with_opener| will be true and
  // |params.opener_frame_route_id| will be MSG_ROUTING_NONE.
  // When |params.proxy_routing_id| instead of |params.main_frame_routing_id| is
  // specified, a RenderFrameProxy will be created for this RenderView's main
  // RenderFrame.
  // The opener should provide a non-null value for |show_callback| if it needs
  // to send an additional IPC to finish making this view visible.
  static RenderViewImpl* Create(
      AgentSchedulingGroup& agent_scheduling_group,
      CompositorDependencies* compositor_deps,
      mojom::CreateViewParamsPtr params,
      bool was_created_by_renderer,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Instances of this object are created by and destroyed by the browser
  // process. This method must be called exactly once by the IPC subsystem when
  // the browser wishes the object to be destroyed.
  void Destroy();

  // Returns the RenderViewImpl for the given routing ID.
  static RenderViewImpl* FromRoutingID(int routing_id);

  // When true, a hint to all RenderWidgets that they will never be
  // user-visible and thus never need to produce pixels for display. This is
  // separate from page visibility, as background pages can be marked visible in
  // blink even though they are not user-visible. Page visibility controls blink
  // behaviour for javascript, timers, and such to inform blink it is in the
  // foreground or background. Whereas this bit refers to user-visibility and
  // whether the tab needs to produce pixels to put on the screen at some point
  // or not.
  bool widgets_never_composited() const { return widgets_never_composited_; }

  void set_send_content_state_immediately(bool value) {
    send_content_state_immediately_ = value;
  }

  CompositorDependencies* compositor_deps() const { return compositor_deps_; }

  // Functions to add and remove observers for this object.
  void AddObserver(RenderViewObserver* observer);
  void RemoveObserver(RenderViewObserver* observer);

  // Passes along the page zoom to the WebView to set it on a newly attached
  // LocalFrame.
  void PropagatePageZoomToNewlyAttachedFrame(bool use_zoom_for_dsf,
                                             float device_scale_factor);

  // Starts a timer to send an UpdateState message on behalf of |frame|, if the
  // timer isn't already running. This allows multiple state changing events to
  // be coalesced into one update.
  void StartNavStateSyncTimerIfNecessary(RenderFrameImpl* frame);

  // Registers a watcher to observe changes in the
  // blink::RendererPreferences.
  void RegisterRendererPreferenceWatcher(
      mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher);

  // Returns the current instance of blink::RendererPreferences.
  const blink::RendererPreferences& GetRendererPreferences() const;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // blink::WebViewClient implementation --------------------------------------

  blink::WebView* CreateView(
      blink::WebLocalFrame* creator,
      const blink::WebURLRequest& request,
      const blink::WebWindowFeatures& features,
      const blink::WebString& frame_name,
      blink::WebNavigationPolicy policy,
      network::mojom::WebSandboxFlags sandbox_flags,
      const blink::SessionStorageNamespaceId& session_storage_namespace_id,
      bool& consumed_user_gesture,
      const base::Optional<blink::WebImpression>& impression) override;
  blink::WebPagePopup* CreatePopup(blink::WebLocalFrame* creator) override;
  void PrintPage(blink::WebLocalFrame* frame) override;
  bool AcceptsLoadDrops() override;
  bool CanUpdateLayout() override;
  void OnPageVisibilityChanged(PageVisibilityState visibility) override;
  void OnPageFrozenChanged(bool frozen) override;
  void DidUpdateRendererPreferences() override;

  // RenderView implementation -------------------------------------------------

  bool Send(IPC::Message* message) override;
  RenderFrameImpl* GetMainRenderFrame() override;
  int GetRoutingID() override;
  blink::WebView* GetWebView() override;

  // Please do not add your stuff randomly to the end here. If there is an
  // appropriate section, add it there. If not, there are some random functions
  // nearer to the top you can add it to.

  bool renderer_wide_named_frame_lookup() {
    return renderer_wide_named_frame_lookup_;
  }

 protected:
  RenderViewImpl(AgentSchedulingGroup& agent_scheduling_group,
                 CompositorDependencies* compositor_deps,
                 const mojom::CreateViewParams& params);
  ~RenderViewImpl() override;

 private:
  // For unit tests.
  friend class RenderViewImplTest;
  friend class RenderViewTest;

  // TODO(nasko): Temporarily friend RenderFrameImpl, so we don't duplicate
  // utility functions needed in both classes, while we move frame specific
  // code away from this class.
  friend class RenderFrameImpl;

  // Initialize() is separated out from the constructor because it is possible
  // to accidentally call virtual functions. All RenderViewImpl creation is
  // fronted by the Create() method which ensures Initialize() is always called
  // before any other code can interact with instances of this call.
  void Initialize(CompositorDependencies* compositor_deps,
                  mojom::CreateViewParamsPtr params,
                  bool was_created_by_renderer,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  static WindowOpenDisposition NavigationPolicyToDisposition(
      blink::WebNavigationPolicy policy);

  // Misc private functions ----------------------------------------------------

#if defined(OS_ANDROID)
  // Make the video capture devices (e.g. webcam) stop/resume delivering video
  // frames to their clients, depending on flag |suspend|. This is called in
  // response to a RenderView PageHidden/Shown().
  void SuspendVideoCaptureDevices(bool suspend);
#endif

  // In OOPIF-enabled modes, this tells each RenderFrame with a pending state
  // update to inform the browser process.
  void SendFrameStateUpdates();

  // ---------------------------------------------------------------------------
  // ADDING NEW FUNCTIONS? Please keep private functions alphabetized and put
  // it in the same order in the .cc file as it was in the header.
  // ---------------------------------------------------------------------------

  // Becomes true when Destroy() is called.
  bool destroying_ = false;

  // Routing ID that allows us to communicate with the corresponding
  // RenderViewHost in the parent browser process.
  const int32_t routing_id_;

  // Whether lookup of frames in the created RenderView (e.g. lookup via
  // window.open or via <a target=...>) should be renderer-wide (i.e. going
  // beyond the usual opener-relationship-based BrowsingInstance boundaries).
  const bool renderer_wide_named_frame_lookup_;

  // A value provided by the browser to state that all RenderWidgets in this
  // RenderView's frame tree will never be user-visible and thus never need to
  // produce pixels for display. This is separate from Page visibility, as
  // non-user-visible pages can still be marked visible for blink. Page
  // visibility controls blink behaviour for javascript, timers, and such to
  // inform blink it is in the foreground or background. Whereas this bit refers
  // to user-visibility and whether the tab needs to produce pixels to put on
  // the screen at some point or not.
  const bool widgets_never_composited_;

  // Dependency injection for RenderWidget and compositing to inject behaviour
  // and not depend on RenderThreadImpl in tests.
  CompositorDependencies* const compositor_deps_;

  // Settings ------------------------------------------------------------------

  // Whether content state (such as form state, scroll position and page
  // contents) should be sent to the browser immediately. This is normally
  // false, but set to true by some tests.
  bool send_content_state_immediately_ = false;

  // Loading state -------------------------------------------------------------

  // Timer used to delay the updating of nav state (see
  // StartNavStateSyncTimerIfNecessary).
  base::OneShotTimer nav_state_sync_timer_;

  // Set of RenderFrame routing IDs for frames that having pending UpdateState
  // messages to send when the next |nav_state_sync_timer_| fires.
  std::set<int> frames_with_pending_state_;

  // View ----------------------------------------------------------------------

  // This class owns this member, and is responsible for calling
  // WebView::Close().
  blink::WebView* webview_ = nullptr;

  // Helper objects ------------------------------------------------------------

  // The `AgentSchedulingGroup` this view is associated with.
  AgentSchedulingGroup& agent_scheduling_group_;

  RenderFrameImpl* main_render_frame_ = nullptr;

#if defined(OS_ANDROID)
  // Android Specific ----------------------------------------------------------

  // Whether this was a renderer-created or browser-created RenderView.
  bool was_created_by_renderer_ = false;
#endif

  // Misc ----------------------------------------------------------------------

  // All the registered observers.  We expect this list to be small, so vector
  // is fine.
  base::ObserverList<RenderViewObserver>::Unchecked observers_;

  // ---------------------------------------------------------------------------
  // ADDING NEW DATA? Please see if it fits appropriately in one of the above
  // sections rather than throwing it randomly at the end. If you're adding a
  // bunch of stuff, you should probably create a helper class and put your
  // data and methods on that to avoid bloating RenderView more.  You can
  // use the Observer interface to filter IPC messages and receive frame change
  // notifications.
  // ---------------------------------------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(RenderViewImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
