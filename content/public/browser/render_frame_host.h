// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/browser_controls_state.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/page_visibility_state.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/common/sudden_termination_disabler_type.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom-forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {
class AssociatedInterfaceProvider;
struct MediaPlayerAction;
namespace mojom {
enum class FeaturePolicyFeature;
}  // namespace mojom
}  // namespace blink

namespace base {
class UnguessableToken;
class Value;
}

namespace features {
CONTENT_EXPORT extern const base::Feature kCrashReporting;
}  // namespace features

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

  // Globally allows for injecting JavaScript into the main world. This feature
  // is present only to support Android WebView, WebLayer, Fuchsia web.Contexts,
  // and CastOS content shell. It must not be used in other configurations.
  static void AllowInjectingJavaScript();

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

  // Returns the process for this frame.
  virtual RenderProcessHost* GetProcess() = 0;

  // Returns the RenderWidgetHostView that can be used to control focus and
  // visibility for this frame.
  virtual RenderWidgetHostView* GetView() = 0;

  // Returns the current RenderFrameHost of the parent frame, or nullptr if
  // there is no parent. The result may be in a different process than the
  // current RenderFrameHost.
  virtual RenderFrameHost* GetParent() = 0;

  // Returns a vector of all RenderFrameHosts in the subtree rooted at |this|.
  // The results may be in different processes.
  // TODO(https://crbug.com/1013740): Consider exposing a way for the browser
  // process to run a function across a subtree in all renderers rather than
  // exposing the RenderFrameHosts of the frames here.
  virtual std::vector<RenderFrameHost*> GetFramesInSubtree() = 0;

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

  // Returns true if the frame is display: none.
  virtual bool IsFrameDisplayNone() = 0;

  // Returns the size of the frame in the viewport. The frame may not be aware
  // of its size.
  virtual const base::Optional<gfx::Size>& GetFrameSize() = 0;

  // Returns the distance from this frame to the root frame.
  virtual size_t GetFrameDepth() = 0;

  // Returns true if the frame is out of process.
  virtual bool IsCrossProcessSubframe() = 0;

  // Returns the last committed URL of the frame.
  virtual const GURL& GetLastCommittedURL() = 0;

  // Returns the last committed origin of the frame.
  virtual const url::Origin& GetLastCommittedOrigin() = 0;

  // Returns the associated widget's native view.
  virtual gfx::NativeView GetNativeView() = 0;

  // Adds |message| to the DevTools console.
  virtual void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                                   const std::string& message) = 0;

  // Functions to run JavaScript in this frame's context. Pass in a callback to
  // receive a result when it is available. If there is no need to receive the
  // result, pass in a default-constructed callback. If provided, the callback
  // will be invoked on the UI thread.
  using JavaScriptResultCallback = base::OnceCallback<void(base::Value)>;

  // This is the default API to run JavaScript in this frame. This API can only
  // be called on chrome:// or devtools:// URLs.
  virtual void ExecuteJavaScript(const base::string16& javascript,
                                 JavaScriptResultCallback callback) = 0;

  // This runs the JavaScript in an isolated world of the top of this frame's
  // context.
  virtual void ExecuteJavaScriptInIsolatedWorld(
      const base::string16& javascript,
      JavaScriptResultCallback callback,
      int32_t world_id) = 0;

  // This runs the JavaScript, but without restrictions. THIS IS ONLY FOR TESTS.
  virtual void ExecuteJavaScriptForTests(
      const base::string16& javascript,
      JavaScriptResultCallback callback,
      int32_t world_id = ISOLATED_WORLD_ID_GLOBAL) = 0;

  // This runs the JavaScript, but without restrictions. THIS IS ONLY FOR TESTS.
  // This version adds a fake UserGestureIndicator to test functionality that
  // requires such a user gesture. https://crbug.com/408426
  virtual void ExecuteJavaScriptWithUserGestureForTests(
      const base::string16& javascript,
      int32_t world_id = ISOLATED_WORLD_ID_GLOBAL) = 0;

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
  // frame coordinates). If there is a data-URL-based image at the location, the
  // renderer will post back the appropriate download message to trigger the
  // save UI.  Nothing gets done if there is no image at that location (or if
  // the image has a non-data URL).
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
  virtual PageVisibilityState GetVisibilityState() = 0;

  // Returns true if WebContentsObserver::RenderFrameCreate notification has
  // been dispatched for this frame, and so a RenderFrameDeleted notification
  // will later be dispatched for this frame.
  virtual bool IsRenderFrameCreated() = 0;

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
  virtual size_t GetProxyCount() = 0;

  // Returns true if the frame has a selection.
  virtual bool HasSelection() = 0;

  // Text surrounding selection.
  virtual void RequestTextSurroundingSelection(
      blink::mojom::LocalFrame::GetTextSurroundingSelectionCallback callback,
      int max_length) = 0;

  // Generates an intervention report in this frame.
  virtual void SendInterventionReport(const std::string& id,
                                      const std::string& message) = 0;

  // Tell the render frame to enable a set of javascript bindings. The argument
  // should be a combination of values from BindingsPolicy.
  virtual void AllowBindings(int binding_flags) = 0;

  // Returns a bitwise OR of bindings types that have been enabled for this
  // RenderFrame. See BindingsPolicy for details.
  virtual int GetEnabledBindings() = 0;

  // Sets a property with the given name and value on the WebUI object
  // associated with this RenderFrameHost, if one exists.
  virtual void SetWebUIProperty(const std::string& name,
                                const std::string& value) = 0;

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
      blink::SuddenTerminationDisablerType disabler_type) = 0;

  // Returns true if the given |threshold_value| is below the threshold value
  // specified in the policy for |feature| for this RenderFrameHost. See
  // third_party/blink/public/common/feature_policy/feature_policy.h for how to
  // compare values of different types. Use this in the browser process to
  // determine whether access to a feature is allowed.
  virtual bool IsFeatureEnabled(blink::mojom::FeaturePolicyFeature feature,
                                blink::PolicyValue threshold_value) = 0;
  // Same as above, with |threshold_value| set to the max value the given
  // |feature| can have.
  virtual bool IsFeatureEnabled(blink::mojom::FeaturePolicyFeature feature) = 0;

  // Opens view-source tab for the document last committed in this
  // RenderFrameHost.
  virtual void ViewSource() = 0;

  // Starts pausing subresource loading on this frame and returns
  // PauseSubresourceLoadingHandle that controls the pausing behavior.  As long
  // as this handle is live, pausing will continue until an internal
  // navigation happens in the frame.
  virtual mojo::Remote<blink::mojom::PauseSubresourceLoadingHandle>
  PauseSubresourceLoading() = 0;

  // Run the given action on the media player location at the given point.
  virtual void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point& location,
      const blink::MediaPlayerAction& action) = 0;

  // Creates a Network Service-backed factory from appropriate |NetworkContext|.
  // If this returns true, any redirect safety checks should be bypassed in
  // downstream loaders.
  virtual bool CreateNetworkServiceDefaultFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver) = 0;

  // Requests that future URLLoaderFactoryBundle(s) sent to the renderer should
  // use a separate URLLoaderFactory for requests initiated by isolated worlds
  // listed in |isolated_world_origins|.  The URLLoaderFactory(s) for each
  // origin will be created via
  // ContentBrowserClient::CreateURLLoaderFactoryForNetworkRequests method.
  virtual void MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      base::flat_set<url::Origin> isolated_world_origins,
      bool push_to_renderer_now) = 0;

  // Returns true if the given sandbox flag |flags| is in effect on this frame.
  // The effective flags include those which have been set by a
  // Content-Security-Policy header, in addition to those which are set by the
  // embedding frame.
  virtual bool IsSandboxed(blink::WebSandboxFlags flags) = 0;

  // Calls |FlushForTesting()| on Network Service and FrameNavigationControl
  // related interfaces to make sure all in-flight mojo messages have been
  // received by the other end. For test use only.
  virtual void FlushNetworkAndNavigationInterfacesForTesting() = 0;

  using PrepareForInnerWebContentsAttachCallback =
      base::OnceCallback<void(RenderFrameHost*)>;
  // This API is used to provide the caller with a RenderFrameHost which is safe
  // for usage in WebContents::AttachToOuterWebContentsFrame API. The final
  // frame returned with |callback| will share the same FrameTreeNodeId with
  // this RenderFrameHost but might not necessarily be the same RenderFrameHost.
  // IMPORTANT: This method can only be called on a child frame. It does not
  // make sense to attach an inner WebContents to the outer WebContents main
  // frame.
  // Essentially, this method will:
  //  1- Cancel any ongoing navigation and navigation requests for this frame.
  //  2- Dispatch beforeunload event on this frame and all of the frame's
  //     subframes, and wait for all beforeunload events to complete.
  //  3- Will create and return a new RenderFrameHost (destroying this one) if
  //     this RenderFrameHost is a cross-process subframe.
  // After steps 1-3 are completed, the callback is invoked asynchronously with
  // the RenderFrameHost which can be safely used for attaching. This
  // RenderFrameHost could be different than |this| which is the case if this
  // RenderFrameHost is for a cross-process frame. The callback could also be
  // invoked with nullptr. This happens if:
  //  1- This frame has beforeunload handlers under it and the user decides to
  //     remain on the page in response to beforeunload prompt.
  //  2- Preparations happened successfully but the frame was somehow removed (
  //     e.g. parent frame detached).
  virtual void PrepareForInnerWebContentsAttach(
      PrepareForInnerWebContentsAttachCallback callback) = 0;

  // Re-creates loader factories and pushes them to |RenderFrame|.
  // Used in case we need to add or remove intercepting proxies to the
  // running renderer, or in case of Network Service connection errors.
  virtual void UpdateSubresourceLoaderFactories() = 0;

  // Returns the type of frame owner element for the FrameTreeNode associated
  // with this RenderFrameHost (e.g., <iframe>, <object>, etc). Note that it
  // returns blink::FrameOwnerElementType::kNone if the RenderFrameHost is a
  // main frame.
  virtual blink::FrameOwnerElementType GetFrameOwnerElementType() = 0;

  // Returns the transient bit of the User Activation v2 state of the
  // FrameTreeNode associated with this RenderFrameHost.
  virtual bool HasTransientUserActivation() = 0;

  // Notifies the renderer whether hiding/showing the browser controls is
  // enabled, what the current state should be, and whether or not to animate to
  // the proper state.
  virtual void UpdateBrowserControlsState(BrowserControlsState constraints,
                                          BrowserControlsState current,
                                          bool animate) = 0;

  // Reloads the frame if it is live. It initiates a reload but doesn't wait for
  // it to finish.
  virtual void Reload() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameHostImpl;
  RenderFrameHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
