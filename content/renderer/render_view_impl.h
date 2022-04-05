// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
#define CONTENT_RENDERER_RENDER_VIEW_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/id_map.h"
#include "base/gtest_prod_util.h"
#include "base/process/process.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom-forward.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_view.h"
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
class RenderFrameImpl;
class RenderViewImplTest;
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
      mojom::CreateViewParamsPtr params,
      bool was_created_by_renderer,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  RenderViewImpl(const RenderViewImpl&) = delete;
  RenderViewImpl& operator=(const RenderViewImpl&) = delete;

  // Instances of this object are created by and destroyed by the browser
  // process. This method must be called exactly once by the IPC subsystem when
  // the browser wishes the object to be destroyed.
  void Destroy();

  // Returns the RenderViewImpl for the given routing ID.
  static RenderViewImpl* FromRoutingID(int routing_id);

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
      const absl::optional<blink::WebImpression>& impression) override;

  // RenderView implementation -------------------------------------------------

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
  void Initialize(mojom::CreateViewParamsPtr params,
                  bool was_created_by_renderer,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  static WindowOpenDisposition NavigationPolicyToDisposition(
      blink::WebNavigationPolicy policy);

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

  // Settings ------------------------------------------------------------------

  // View ----------------------------------------------------------------------

  // This class owns this member, and is responsible for calling
  // WebView::Close().
  blink::WebView* webview_ = nullptr;

  // Helper objects ------------------------------------------------------------

  // The `AgentSchedulingGroup` this view is associated with.
  AgentSchedulingGroup& agent_scheduling_group_;

#if BUILDFLAG(IS_ANDROID)
  // Android Specific ----------------------------------------------------------

  // Whether this was a renderer-created or browser-created RenderView.
  bool was_created_by_renderer_ = false;
#endif

  // ---------------------------------------------------------------------------
  // ADDING NEW DATA? Please see if it fits appropriately in one of the above
  // sections rather than throwing it randomly at the end. If you're adding a
  // bunch of stuff, you should probably create a helper class and put your
  // data and methods on that to avoid bloating RenderView more.  You can
  // use the Observer interface to filter IPC messages and receive frame change
  // notifications.
  // ---------------------------------------------------------------------------
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
