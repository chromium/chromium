// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/console_message_level.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"
#include "third_party/blink/public/platform/web_sudden_termination_disabler_type.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
class AssociatedInterfaceProvider;
struct WebMediaPlayerAction;
namespace mojom {
enum class FeaturePolicyFeature;
}  // namespace mojom
}  // namespace blink

namespace base {
class UnguessableToken;
class Value;
}

namespace resource_coordinator {
class FrameResourceCoordinator;
}

namespace service_manager {
class InterfaceProvider;
}

namespace ui {
struct AXActionData;
}

namespace content {

class RenderProcessHost;
class RenderViewHost;
class RenderWidgetHostView;
class SiteInstance;

// The interface provides a communication conduit with a frame in the renderer.
class CONTENT_EXPORT RenderFrameHost : public IPC::Listener,
                                       public IPC::Sender {
 public:
  // Constant used to denote that a lookup of a FrameTreeNode ID has failed.
  static const int kNoFrameTreeNodeId = -1;

  // Returns the RenderFrameHost given its ID and the ID of its render process.
  // Returns nullptr if the IDs do not correspond to a live RenderFrameHost.
  static RenderFrameHost* FromID(int render_process_id, int render_frame_id);

#if defined(OS_ANDROID)
  // Globally allows for injecting JavaScript into the main world. This feature
  // is present only to support Android WebView and must not be used in other
  // configurations.
  static void AllowInjectingJavaScriptForAndroidWebView();
#endif

  // Returns a RenderFrameHost given its accessibility tree ID.
  static RenderFrameHost* FromAXTreeID(ui::AXTreeID ax_tree_id);

  // Returns the FrameTreeNode ID corresponding to the specified |process_id|
  // and |routing_id|. This routing ID pair may represent a placeholder for
  // frame that is currently rendered in a different process than |process_id|.
  static int GetFrameTreeNodeIdForRoutingId(int process_id, int routing_id);

  // Returns the RenderFrameHost corresponding to the |placeholder_routing_id|
  // in the given |render_process_id|. The returned RenderFrameHost will always
  // be in a different process.  It may be null if the placeholder is not found
  // in the given process, which may happen if the frame was recently deleted
  // or swapped to |render_process_id| itself.
  static RenderFrameHost* FromPlaceholderId(int render_process_id,
                                            int placeholder_routing_id);

  ~RenderFrameHost() override {}

  // Returns the route id for this frame.
  virtual int GetRoutingID() = 0;

  // Returns the accessibility tree ID for this RenderFrameHost.
  virtual ui::AXTreeID GetAXTreeID() = 0;

  // Returns the SiteInstance grouping all RenderFrameHosts that have script
  // access to this RenderFrameHost, and must therefore live in the same
  // process.
  virtual SiteInstance* GetSiteInstance() = 0;

  // Returns the interface for the Global Resource Coordinator
  // for this frame.
  virtual resource_coordinator::FrameResourceCoordinator*
  GetFrameResourceCoordinator() = 0;

  // Returns the process for this frame.
  virtual RenderProcessHost* GetProcess() = 0;

  // Returns the RenderWidgetHostView that can be used to control focus and
  // visibility for this frame.
  virtual RenderWidgetHostView* GetView() = 0;

  // Returns the current RenderFrameHost of the parent frame, or nullptr if
  // there is no parent. The result may be in a different process than the
  // current RenderFrameHost.
  virtual RenderFrameHost* GetParent() = 0;

  // Returns whether or not this RenderFrameHost is a descendant of |ancestor|.
  // This is equivalent to check that |ancestor| is reached by iterating on
  // GetParent().
  // This is a strict relationship, a RenderFrameHost is never an ancestor of
  // itself.
  virtual bool IsDescendantOf(RenderFrameHost* ancestor) = 0;

  // Returns the FrameTreeNode ID for this frame. This ID is browser-global and
  // uniquely identifies a frame that hosts content. The identifier is fixed at
  // the creation of the frame and stays constant for the lifetime of the frame.
  // When the frame is removed, the ID is not used again.
  //
  // A RenderFrameHost is tied to a process. Due to cross-process navigations,
  // the RenderFrameHost may have a shorter lifetime than a frame. Consequently,
  // the same FrameTreeNode ID may refer to a different RenderFrameHost after a
  // navigation.
  virtual int GetFrameTreeNodeId() = 0;

  // Used for devtools instrumentation and trace-ability. The token is
  // propagated to Blink's LocalFrame and both Blink and content/
  // can tag calls and requests with this token in order to attribute them
  // to the context frame. The token is only defined by the browser process and
  // is never sent back from the renderer in the control calls. It should be
  // never used to look up the FrameTreeNode instance.
  virtual base::UnguessableToken GetDevToolsFrameToken() = 0;

  // Returns the assigned name of the frame, the name of the iframe tag
  // declaring it. For example, <iframe name="framename">[...]</iframe>. It is
  // quite possible for a frame to have no name, in which case GetFrameName will
  // return an empty string.
  virtual const std::string& GetFrameName() = 0;

  // Returns true if the frame is out of process.
  virtual bool IsCrossProcessSubframe() = 0;

  // Returns the last committed URL of the frame.
  virtual const GURL& GetLastCommittedURL() = 0;

  // Returns the last committed origin of the frame.
  virtual const url::Origin& GetLastCommittedOrigin() = 0;

  // Returns the associated widget's native view.
  virtual gfx::NativeView GetNativeView() = 0;

  // Adds |message| to the DevTools console.
  virtual void AddMessageToConsole(ConsoleMessageLevel level,
                                   const std::string& message) = 0;

  // Runs some JavaScript in this frame's context. If a callback is provided, it
  // will be used to return the result, when the result is available.
  // This API can only be called on chrome:// or chrome-devtools:// URLs.
  typedef base::Callback<void(const base::Value*)> JavaScriptResultCallback;
  virtual void ExecuteJavaScript(const base::string16& javascript) = 0;
  virtual void ExecuteJavaScript(const base::string16& javascript,
                                 const JavaScriptResultCallback& callback) = 0;

  // Runs some JavaScript in an isolated world of top of this frame's context.
  virtual void ExecuteJavaScriptInIsolatedWorld(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback,
      int world_id) = 0;

  // ONLY FOR TESTS: Same as above but without restrictions. Optionally, adds a
  // fake UserGestureIndicator around execution. (crbug.com/408426)
  virtual void ExecuteJavaScriptForTests(const base::string16& javascript) = 0;
  virtual void ExecuteJavaScriptForTests(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback) = 0;
  virtual void ExecuteJavaScriptWithUserGestureForTests(
      const base::string16& javascript) = 0;

  // Send a message to the RenderFrame to trigger an action on an
  // accessibility object.
  virtual void AccessibilityPerformAction(const ui::AXActionData& data) = 0;

  // This is called when the user has committed to the given find in page
  // request (e.g. by pressing enter or by clicking on the next / previous
  // result buttons). It triggers sending a native accessibility event on
  // the result object on the page, navigating assistive technology to that
  // result.
  virtual void ActivateFindInPageResultForAccessibility(int request_id) = 0;

  // Roundtrips through the renderer and compositor pipeline to ensure that any
  // changes to the contents resulting from operations executed prior to this
  // call are visible on screen. The call completes asynchronously by running
  // the supplied |callback| with a value of true upon successful completion and
  // false otherwise (when the frame is destroyed, detached, etc..).
  using VisualStateCallback = base::OnceCallback<void(bool)>;
  virtual void InsertVisualStateCallback(VisualStateCallback callback) = 0;

  // Copies the image at the location in viewport coordinates (not frame
  // coordinates) to the clipboard. If there is no image at that location, does
  // nothing.
  virtual void CopyImageAt(int x, int y) = 0;

  // Requests to save the image at the location in viewport coordinates (not
  // frame coordinates). If there is an image at the location, the renderer
  // will post back the appropriate download message to trigger the save UI.
  // If there is no image at that location, does nothing.
  virtual void SaveImageAt(int x, int y) = 0;

  // RenderViewHost for this frame.
  virtual RenderViewHost* GetRenderViewHost() = 0;

  // Returns the InterfaceProvider that this process can use to bind
  // interfaces exposed to it by the application running in this frame.
  virtual service_manager::InterfaceProvider* GetRemoteInterfaces() = 0;

  // Returns the AssociatedInterfaceProvider that this process can use to access
  // remote frame-specific Channel-associated interfaces for this frame.
  virtual blink::AssociatedInterfaceProvider*
  GetRemoteAssociatedInterfaces() = 0;

  // Returns the visibility state of the frame. The different visibility states
  // of a frame are defined in Blink.
  virtual blink::mojom::PageVisibilityState GetVisibilityState() = 0;

  // Returns whether the RenderFrame in the renderer process has been created
  // and still has a connection.  This is valid for all frames.
  virtual bool IsRenderFrameLive() = 0;

  // Returns true if this is the currently-visible RenderFrameHost for our frame
  // tree node. During process transfer, a RenderFrameHost may be created that
  // is not current. After process transfer, the old RenderFrameHost becomes
  // non-current until it is deleted (which may not happen until its unload
  // handler runs).
  //
  // Changes to the IsCurrent() state of a RenderFrameHost may be observed via
  // WebContentsObserver::RenderFrameHostChanged().
  virtual bool IsCurrent() = 0;

  // Get the number of proxies to this frame, in all processes. Exposed for
  // use by resource metrics.
  virtual int GetProxyCount() = 0;

  // Returns true if the frame has a selection.
  virtual bool HasSelection() = 0;

  // Text surrounding selection.
  typedef base::Callback<
      void(const base::string16& content, int start_offset, int end_offset)>
      TextSurroundingSelectionCallback;
  virtual void RequestTextSurroundingSelection(
      const TextSurroundingSelectionCallback& callback,
      int max_length) = 0;

  // Tell the render frame to enable a set of javascript bindings. The argument
  // should be a combination of values from BindingsPolicy.
  virtual void AllowBindings(int binding_flags) = 0;

  // Returns a bitwise OR of bindings types that have been enabled for this
  // RenderFrame. See BindingsPolicy for details.
  virtual int GetEnabledBindings() const = 0;

#if defined(OS_ANDROID)
  // Returns an InterfaceProvider for Java-implemented interfaces that are
  // scoped to this RenderFrameHost. This provides access to interfaces
  // implemented in Java in the browser process to C++ code in the browser
  // process.
  virtual service_manager::InterfaceProvider* GetJavaInterfaces() = 0;
#endif  // OS_ANDROID

  // Stops and disables the hang monitor for beforeunload. This avoids flakiness
  // in tests that need to observe beforeunload dialogs, which could fail if the
  // timeout skips the dialog.
  virtual void DisableBeforeUnloadHangMonitorForTesting() = 0;
  virtual bool IsBeforeUnloadHangMonitorDisabledForTesting() = 0;

  // Check whether the specific Blink feature is currently preventing fast
  // shutdown of the frame.
  virtual bool GetSuddenTerminationDisablerState(
      blink::WebSuddenTerminationDisablerType disabler_type) = 0;

  // Returns true if the given Feature Policy |feature| is enabled for this
  // RenderFrameHost and is allowed to be used by it. Use this in the browser
  // process to determine whether access to a feature is allowed.
  virtual bool IsFeatureEnabled(blink::mojom::FeaturePolicyFeature feature) = 0;

  // Opens view-source tab for the document last committed in this
  // RenderFrameHost.
  virtual void ViewSource() = 0;

  // Starts pausing subresource loading on this frame and returns
  // PauseSubresourceLoadingHandle that controls the pausing behavior.  As long
  // as this handle is live, pausing will continue until an internal
  // navigation happens in the frame.
  virtual blink::mojom::PauseSubresourceLoadingHandlePtr
  PauseSubresourceLoading() = 0;

  // Run the given action on the media player location at the given point.
  virtual void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point& location,
      const blink::WebMediaPlayerAction& action) = 0;

  // Creates a Network Service-backed factory from appropriate |NetworkContext|.
  // If this returns true, any redirect safety checks should be bypassed in
  // downstream loaders.
  virtual bool CreateNetworkServiceDefaultFactory(
      network::mojom::URLLoaderFactoryRequest default_factory_request) = 0;

  // Requests that future URLLoaderFactoryBundle(s) sent to the renderer should
  // use a separate URLLoaderFactory for requests initiated by any of the
  // origins listed in |request_initiators|.  The URLLoaderFactory(s) for each
  // origin will be created via
  // ContentBrowserClient::CreateURLLoaderFactoryForNetworkRequests method.
  virtual void MarkInitiatorsAsRequiringSeparateURLLoaderFactory(
      std::vector<url::Origin> request_initiators,
      bool push_to_renderer_now) = 0;

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameHostImpl;
  RenderFrameHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
