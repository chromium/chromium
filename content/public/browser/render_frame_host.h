// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "base/memory/safety_checks.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/common/content_export.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/web_exposed_isolation_level.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/extra_mojo_js_features.mojom.h"
#include "content/public/common/isolated_world_ids.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "net/cookies/cookie_setting_override.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-forward.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_declaration.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-forward.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/sudden_termination_disabler_type.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-forward.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom-forward.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-forward.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/jni_zero/jni_zero.h"
#endif

class GURL;

namespace base {
class UnguessableToken;
}  // namespace base

namespace blink {
class AssociatedInterfaceProvider;
class PermissionsPolicy;
class StorageKey;

namespace mojom {
enum class AuthenticatorStatus;
enum class PermissionsPolicyFeature;
class MediaPlayerAction;
}  // namespace mojom
}  // namespace blink

namespace gfx {
class Point;
class Size;
}  // namespace gfx

namespace mojo {
template <typename T>
class PendingReceiver;
}  // namespace mojo

namespace net {
class IsolationInfo;
class NetworkIsolationKey;
}  // namespace net

namespace network {
namespace mojom {
class URLLoaderFactory;
class URLResponseHead;
}
}  // namespace network

namespace perfetto::protos::pbzero {
class RenderFrameHost;
}  // namespace perfetto::protos::pbzero

namespace service_manager {
class InterfaceProvider;
}

namespace ui {
struct AXActionData;
struct AXTreeUpdate;
class AXTreeID;
}  // namespace ui

namespace url {
class Origin;
}

namespace content {

class BrowserContext;
class DocumentRef;
struct GlobalRenderFrameHostId;
struct GlobalRenderFrameHostToken;
class NavigationHandle;
class RenderProcessHost;
class RenderViewHost;
class RenderWidgetHost;
class RenderWidgetHostView;
class SiteInstance;
class StoragePartition;
class WeakDocumentPtr;
class WebUI;
class Page;

// The interface provides a communication conduit with a frame in the renderer.
// The preferred way to keep a reference to a RenderFrameHost is storing a
// GlobalRenderFrameHostId and using RenderFrameHost::FromID() when you need to
// access it.
//
// Any code that uses RenderFrameHost must be aware of back-forward cache, see
// LifecycleState. The main side-effect is that any IPCs that are processed on a
// freezable task queue can stall indefinitely. See
// MainThreadTaskQueue::QueueTraits::can_be_frozen. Code that uses
// RenderFrameHost should refrain from passing this negative externality on to
// higher-level dependencies. In short: code that uses RenderFrameHost must be
// back-forward cache aware, and code that does not use RenderFrameHost should
// not have to be back-forward cache aware.
class CONTENT_EXPORT RenderFrameHost : public IPC::Listener,
                                       public IPC::Sender {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  // Returns the RenderFrameHost given its ID and the ID of its render process.
  // Returns nullptr if the IDs do not correspond to a live RenderFrameHost.
  static RenderFrameHost* FromID(const GlobalRenderFrameHostId& id);
  static RenderFrameHost* FromID(int render_process_id, int render_frame_id);

  // Returns the RenderFrameHost given its global frame token. Returns nullptr
  // if the frame token does not correspond to a live RenderFrameHost.
  static RenderFrameHost* FromFrameToken(
      const GlobalRenderFrameHostToken& frame_token);

  // Globally allows for injecting JavaScript into the main world. This feature
  // is present only to support Android WebView, WebLayer, Fuchsia web.Contexts,
  // and CastOS content shell. It must not be used in other configurations.
  static void AllowInjectingJavaScript();

  // Returns a RenderFrameHost given its accessibility tree ID.
  static RenderFrameHost* FromAXTreeID(const ui::AXTreeID& ax_tree_id);

  // Returns the FrameTreeNode ID corresponding to the specified |process_id|
  // and |routing_id|. This routing ID pair may represent a placeholder for
  // frame that is currently rendered in a different process than |process_id|.
  static FrameTreeNodeId GetFrameTreeNodeIdForRoutingId(int process_id,
                                                        int routing_id);

  // Returns the FrameTreeNode ID corresponding to the specified |process_id|
  // and |frame_token|. This routing ID pair may represent a placeholder for
  // frame that is currently rendered in a different process than |process_id|.
  static FrameTreeNodeId GetFrameTreeNodeIdForFrameToken(
      int process_id,
      const ::blink::FrameToken& frame_token);

  // Returns the RenderFrameHost corresponding to the
  // |placeholder_frame_token| in the given |render_process_id|. The returned
  // RenderFrameHost will always be in a different process.  It may be null if
  // the placeholder is not found in the given process, which may happen if the
  // frame was recently deleted or swapped to |render_process_id| itself.
  static RenderFrameHost* FromPlaceholderToken(
      int render_process_id,
      const blink::RemoteFrameToken& placeholder_frame_token);

#if BUILDFLAG(IS_ANDROID)
  // Returns the RenderFrameHost object associated with a Java native pointer.
  static RenderFrameHost* FromJavaRenderFrameHost(
      const base::android::JavaRef<jobject>& jrender_frame_host_android);
#endif

  // Logs UMA metrics related to isolatable sandboxed iframes.
  static void LogSandboxedIframesIsolationMetrics();

  ~RenderFrameHost() override = default;

  // Returns the storage key for the last committed document in this
  // RenderFrameHost. It is used for partitioning storage by the various
  // storage APIs.
  virtual const blink::StorageKey& GetStorageKey() const = 0;

  // Returns the route id for this frame.
  virtual int GetRoutingID() const = 0;

  // Returns the frame token for this frame.
  virtual const blink::LocalFrameToken& GetFrameToken() const = 0;

  // Returns the reporting source token for the document in this frame. This is
  // used by the Reporting API to associate queued reports generated by this
  // document with the reporting endpoint configuration delivered with the
  // Reporting-Endpoints response header.
  virtual const base::UnguessableToken& GetReportingSource() = 0;

  // Returns the accessibility tree ID for this RenderFrameHost.
  virtual ui::AXTreeID GetAXTreeID() = 0;

  using AXTreeSnapshotCallback = base::OnceCallback<void(ui::AXTreeUpdate&)>;

  // Returns the SiteInstance grouping all RenderFrameHosts that have script
  // access to this RenderFrameHost, and must therefore live in the same
  // process.
  // Associated SiteInstance never changes.
  virtual SiteInstance* GetSiteInstance() const = 0;

  // Returns the process for this frame.
  // Associated RenderProcessHost never changes.
  virtual RenderProcessHost* GetProcess() const = 0;

  // Returns the GlobalRenderFrameHostId for this frame. Embedders should store
  // this instead of a raw RenderFrameHost pointer. This API is based on routing
  // IDs from legacy IPC. The renderer may not have routing IDs for frames so it
  // is preferred to use `GetGlobalFrameToken` over this API.
  virtual GlobalRenderFrameHostId GetGlobalId() const = 0;

  // Returns the GlobalRenderFrameHostToken for this frame. Embedders should
  // store this instead of a raw RenderFrameHost pointer.
  virtual GlobalRenderFrameHostToken GetGlobalFrameToken() const = 0;

  // Returns a StoragePartition associated with this RenderFrameHost.
  // Associated StoragePartition never changes.
  virtual StoragePartition* GetStoragePartition() = 0;

  // Returns the user browser context associated with this RenderFrameHost.
  // Associated BrowserContext never changes.
  virtual BrowserContext* GetBrowserContext() = 0;

  // Returns the current document's response head. Note that this value will
  // change when a cross-document navigation reuses RenderFrameHost and commits
  // a new document in existing RenderFrameHost. Must not be called in
  // LifecycleState::kPendingCommit before committing a document.
  //
  // This is null if there was no response: the initial empty document,
  // about:blank, about:srcdoc, and MHTML iframes.
  virtual const network::mojom::URLResponseHead* GetLastResponseHead() = 0;

  // Returns the RenderWidgetHostView for this frame or the nearest ancestor
  // frame, which can be used to control input, focus, rendering and visibility
  // for this frame.
  // This returns null when there is no connection to a renderer process, which
  // can be checked with IsRenderFrameLive().
  // NOTE: Due to historical relationships between RenderViewHost and
  // RenderWidgetHost, the main frame RenderWidgetHostView may initially exist
  // before IsRenderFrameLive() is true, but they would afterward change
  // values together. It is better to not rely on this behaviour as it is
  // intended to change. See https://crbug.com/419087.
  virtual RenderWidgetHostView* GetView() = 0;

  // Returns the RenderWidgetHost attached to this frame or the nearest ancestor
  // frame, which could potentially be the root. This allows access to the
  // RenderWidgetHost without having to go through GetView() which can be null,
  // so should be preferred to GetView()->GetRenderWidgetHost().
  //
  // This method is not valid to be called when the RenderFrameHost is detached
  // from the frame tree, though this would only happen during destruction of
  // the RenderFrameHost.
  virtual RenderWidgetHost* GetRenderWidgetHost() = 0;

  // Returns the parent of this RenderFrameHost, or nullptr if this
  // RenderFrameHost is the main one and there is no parent.
  // The result may be in a different process than the
  // current RenderFrameHost.
  virtual RenderFrameHost* GetParent() const = 0;

  // Returns the document owning the frame this RenderFrameHost is located
  // in, which will either be a parent (for <iframe>s) or outer document (for
  // <fencedframe>). This will return the outer document in cases
  // of fenced frames but will not cross a browsing session boundary
  // (ie. it will not escape a GuestView). See
  // `RenderFrameHost::GetParentOrOuterDocumentOrEmbedder` for the
  // version of this API that will cross a browsing session boundary.
  // This method typically will be used for permissions and policy decisions
  // based on checking origins.
  // Example:
  //  A
  //   B (iframe)
  //   C (fenced frame - placeholder frame)
  //    C* (main frame in fenced frame).
  //
  //  C* GetParent returns null.
  //  C* GetParentOrOuterDocument returns A.
  //  C GetParent & GetParentOrOuterDocument returns A.
  //  B GetParent & GetParentOrOuterDocument returns A.
  //  A GetParent & GetParentOrOuterDocument returns nullptr.
  virtual RenderFrameHost* GetParentOrOuterDocument() const = 0;

  // Returns the document owning the frame this RenderFrameHost is located
  // in, which will either be a parent (for <iframe>s) or outer document (for
  // <fencedframe>, or an embedder (e.g. GuestViews)). See
  // `RenderFrameHost::GetParentOrOuterDocument` for the version of this API
  // that does not cross a browsing session boundary (ie. Not escaping a
  // GuestView). This method typically will be used for input, compositing, and
  // focus related functionality where the physical arrangement of frames, as
  // opposed to their semantics is required. Example:
  //  A (GuestView embedder)
  //   B (<webview> - placeholder frame)
  //    B* (embedded document main frame)
  //     C (iframe)
  //
  //  C GetParent & GetParentOrOuterDocumentOrEmbedder returns B*.
  //  B* GetParent & GetParentOrOuterDocument returns null.
  //  B* GetParentOrOuterDocumentOrEmbedder returns A.
  //  B GetParent & GetParentOrOuterDocumentOrEmbedder returns A.
  //  A GetParent & GetParentOrOuterDocumentOrEmbedder returns nullptr.
  virtual RenderFrameHost* GetParentOrOuterDocumentOrEmbedder() const = 0;

  // Returns the eldest parent of this RenderFrameHost.
  // Always non-null, but might be equal to |this|.
  // The result may be in a different process that the current RenderFrameHost.
  //
  // NOTE: The result might be different from
  // WebContents::FromRenderFrameHost(this)->GetMainFrame().
  // This function (RenderFrameHost::GetMainFrame) is the preferred API in
  // almost all of the cases. See RenderFrameHost::IsActive for the details.
  virtual RenderFrameHost* GetMainFrame() = 0;

  // Returns true if `this` is the main document of the primary `Page` of the
  // associated WebContents. See the description of
  // `WebContents::GetPrimaryPage` for details. It is therefore also current in
  // the primary main frame.
  virtual bool IsInPrimaryMainFrame() = 0;

  // Returns the topmost ancestor RenderFrameHost of this RenderFrameHost. This
  // includes any parents (in the case of subframes) and any outer documents
  // (e.g. fenced frame owners), but does not traverse out of GuestViews.
  // This can be used instead of GetMainFrame in cases where we want to escape
  // inner pages. See also GetParentOrOuterDocument for more details on the
  // distinction of "parents" and "outer documents."
  // Note that this may be different from getting the WebContents' primary main
  // frame. For example, if `this` is in a bfcached or prerendered page, this
  // will return the cached/prerendered page's main RenderFrameHost.
  // See docs/frame_trees.md for more details.
  virtual RenderFrameHost* GetOutermostMainFrame() = 0;

  // Returns the topmost ancestor RenderFrameHost. This includes any parents (in
  // the case of subframes), any outer documents (e.g. fenced frame owners), and
  // any GuestViews. See also GetOutermostMainFrame which does not escape
  // GuestViews and GetParentOrOuterDocumentOrEmbedder for more details.
  // Note that this may be different from getting the WebContents' primary main
  // frame. For example, if `this` is in a bfcached or prerendered page, this
  // will return the cached/prerendered page's main RenderFrameHost.
  // See docs/frame_trees.md for more details.
  virtual RenderFrameHost* GetOutermostMainFrameOrEmbedder() = 0;

  // Fenced frames (meta-bug https://crbug.com/1111084):
  // Returns true if this document is the root of a fenced frame tree. This
  // supports both Shadow DOM and MPArch implementations.
  //
  // In particular, this always returns false for frames loaded inside a
  // <fencedframe> element, if the frame is not the top-level <fencedframe>
  // itself. That is, this will return false for all <iframes> nested under a
  // <fencedframe>.
  virtual bool IsFencedFrameRoot() const = 0;

  // Fenced frames (meta-bug https://crbug.com/1111084):
  // Returns true if `this` was loaded in a <fencedframe> element directly or if
  // one of `this` ancestors was loaded in a <fencedframe> element. This
  // supports both Shadow DOM and MPArch implementations.
  virtual bool IsNestedWithinFencedFrame() const = 0;

  // Check if the frame has untrusted network access disabled.
  //
  // A Fenced frame can disable untrusted network access for itself and the
  // descendant iframes in the fenced frame tree by calling the fenced frame API
  // `window.fence.disableUntrustedNetwork()`. After this API is invoked, no
  // untrusted network requests are allowed in the fenced frame tree, i.e. in
  // the root fenced frame and all of its descendant iframes. This includes:
  // * Subresources requests.
  // * Navigation requests.
  // * Event level reporting.
  // * Any other network channels, for example, WebSocket, web workers, etc.
  //
  // Fenced frames will get access to cross-site information, for example,
  // shared storage API after the untrusted network access is disabled.
  //
  // Note: An example of a trusted network request is the aggregation report
  // sent by Private Aggregation API. Because the report is privacy preserving,
  // it is allowed from the fenced frame after the untrusted network access is
  // disabled. Additional trusted network communications, such as to a secure
  // trusted execution environment, may be added in the future.
  //
  // See
  // https://github.com/WICG/fenced-frame/blob/master/explainer/fenced_frames_with_local_unpartitioned_data_access.md.
  virtual bool IsUntrustedNetworkDisabled() const = 0;

  // |ForEachRenderFrameHost| traverses this RenderFrameHost and all of its
  // descendants, including frames in any inner frame trees (such as guest
  // views), in breadth-first order.
  //
  // Note: The RenderFrameHost parameter is not guaranteed to have a
  // live RenderFrame counterpart in the renderer process. Callbacks should
  // check IsRenderFrameLive(), as sending IPC messages to it in this case will
  // fail silently.
  //
  // The callback returns a FrameIterationAction which determines if/how
  // iteration on subsequent frames continues. The FrameIterationAction may be
  // omitted, in which case kContinue will be assumed.
  enum class FrameIterationAction {
    // Includes the children of the visited frame for subsequent traversal and
    // continues traversal to the next frame.
    kContinue,
    // Continues traversal to the next frame but does not include the children
    // of the visited frame for subsequent traversal.
    kSkipChildren,
    // Does not continue traversal.
    kStop
  };
  virtual void ForEachRenderFrameHostWithAction(
      base::FunctionRef<FrameIterationAction(RenderFrameHost*)> on_frame) = 0;
  virtual void ForEachRenderFrameHost(
      base::FunctionRef<void(RenderFrameHost*)> on_frame) = 0;

  // Returns the FrameTreeNode ID associated with this RenderFrameHost.
  //
  // If a stable identifier is needed, GetGlobalId() always refers to this
  // RenderFrameHost, while this RenderFrameHost might host multiple documents
  // over its lifetime, and this RenderFrameHost might have a shorter lifetime
  // than the frame hosting content, as explained above. For associating data
  // with a single document, DocumentUserData can be used.
  virtual FrameTreeNodeId GetFrameTreeNodeId() const = 0;

  // Used for devtools instrumentation and trace-ability. The token is
  // propagated to Blink's LocalFrame and both Blink and content/
  // can tag calls and requests with this token in order to attribute them
  // to the context frame. The token is only defined by the browser process and
  // is never sent back from the renderer in the control calls. It should be
  // never used to look up the FrameTreeNode instance.
  virtual const base::UnguessableToken& GetDevToolsFrameToken() = 0;

  // This token is present on all frames. For frames with parents, it allows
  // identification of embedding relationships between parent and child. For
  // main frames, it also allows generalization of the embedding relationship
  // when the WebContents itself is embedded in another context such as the rest
  // of the browser UI. This will be nullopt prior to the RenderFrameHost
  // committing a navigation. After the first navigation commits this
  // will return the token for the last committed document.
  //
  // TODO(crbug.com/40136951): Remove the nullopt scenario by creating the token
  // in CreateChildFrame() or similar.
  virtual std::optional<base::UnguessableToken> GetEmbeddingToken() = 0;

  // Returns the assigned name of the frame, the name of the iframe tag
  // declaring it. For example, <iframe name="framename">[...]</iframe>. It is
  // quite possible for a frame to have no name, in which case GetFrameName will
  // return an empty string.
  virtual const std::string& GetFrameName() = 0;

  // Returns true if the frame is display: none.
  virtual bool IsFrameDisplayNone() = 0;

  // Returns the size of the frame in the viewport. The frame may not be aware
  // of its size.
  virtual const std::optional<gfx::Size>& GetFrameSize() = 0;

  // Returns the distance from this frame to its main frame.
  virtual size_t GetFrameDepth() = 0;

  // Returns true if the frame is out of process relative to its parent.
  virtual bool IsCrossProcessSubframe() = 0;

  // Returns the cross-origin isolation capability of this frame.
  //
  // Note that this is a property of the document and can change as the frame
  // navigates.
  //
  // Unlike RenderProcessHost::GetWebExposedIsolationLevel(), this takes the
  // currently document's Permissions Policy into account and may return a
  // lower isolation level than RenderProcessHost if the
  // "cross-origin-isolated" feature is not delegated to this frame. Because
  // of this, this function should generally be used instead of
  // RenderProcessHost::GetWebExposedIsolationLevel() when making decisions
  // based on the isolation level, such as API availability.
  //
  // Note that the embedder can force-enable APIs in processes even if they
  // lack the necessary privilege. This function doesn't account for that; use
  // content::HasIsolatedContextCapability(RenderFrameHost*) to handle this
  // case.
  //
  // TODO(https://936696): Once RenderDocument ships this should be exposed as
  // an invariant of the document host.
  virtual WebExposedIsolationLevel GetWebExposedIsolationLevel() = 0;

  // Returns the last committed URL of this RenderFrameHost. This will be empty
  // until the first commit in this RenderFrameHost.
  //
  // Note that this does not reflect navigations in other RenderFrameHosts,
  // frames, or pages within the same WebContents, so it may differ from
  // NavigationController::GetLastCommittedEntry().
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Returns the last committed origin of this RenderFrameHost.
  virtual const url::Origin& GetLastCommittedOrigin() const = 0;

  // Returns the network isolation key used for subresources from the currently
  // committed navigation. It's set on commit and does not change until the next
  // navigation is committed.
  //
  // TODO(mmenke): Remove this in favor of GetIsolationInfoForSubresoruces().
  virtual const net::NetworkIsolationKey& GetNetworkIsolationKey() = 0;

  // Returns the IsolationInfo used for subresources from the currently
  // committed navigation. It's set on commit and does not change until the next
  // navigation is committed.
  virtual const net::IsolationInfo& GetIsolationInfoForSubresources() = 0;

  // Returns the IsolationInfo used for subresources for the pending commit, if
  // there is one. Otherwise, returns the IsolationInfo used for subresources of
  // the last committed page load.
  //
  // TODO(https://936696): Remove this once RenderDocument ships, at which point
  // it will no longer be needed.
  virtual net::IsolationInfo GetPendingIsolationInfoForSubresources() = 0;

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

  // This API allows to execute JavaScript methods in this frame, without
  // having to serialize the arguments into a single string, and is a lot
  // cheaper than ExecuteJavaScript below since it avoids the need to compile
  // and evaluate new scripts all the time.
  //
  // Calling
  //
  //   ExecuteJavaScriptMethod("obj", "foo", [1, true], callback)
  //
  // is semantically equivalent to
  //
  //   ExecuteJavaScript("obj.foo(1, true)", callback)
  virtual void ExecuteJavaScriptMethod(const std::u16string& object_name,
                                       const std::u16string& method_name,
                                       base::Value::List arguments,
                                       JavaScriptResultCallback callback) = 0;

  // This is the default API to run JavaScript in this frame. This API can only
  // be called on chrome:// or devtools:// URLs.
  virtual void ExecuteJavaScript(const std::u16string& javascript,
                                 JavaScriptResultCallback callback) = 0;

  // This runs the JavaScript in an isolated world of the top of this frame's
  // context. It is invalid to specify a `world_id` of
  // `ISOLATED_WORLD_ID_GLOBAL`.
  virtual void ExecuteJavaScriptInIsolatedWorld(
      const std::u16string& javascript,
      JavaScriptResultCallback callback,
      int32_t world_id) = 0;

  // This runs the JavaScript, but without restrictions. Specify a `world_id` of
  // `ISOLATED_WORLD_ID_GLOBAL` to run the code in the global world. THIS IS
  // ONLY FOR TESTS.
  virtual void ExecuteJavaScriptForTests(const std::u16string& javascript,
                                         JavaScriptResultCallback callback,
                                         int32_t world_id) = 0;

  // This runs the JavaScript, but without restrictions. Unlike the method
  // above, this one triggers a fake user activation notification to test
  // functionalities that are gated by user activation. Specify a `world_id` of
  // `ISOLATED_WORLD_ID_GLOBAL` to run the code in the global world. THIS IS
  // ONLY FOR TESTS.
  virtual void ExecuteJavaScriptWithUserGestureForTests(
      const std::u16string& javascript,
      JavaScriptResultCallback callback,
      int32_t world_id) = 0;

  // Tells the renderer to perform a given action on the plugin located at a
  // given location in its local view coordinate space.
  virtual void ExecutePluginActionAtLocalLocation(
      const gfx::Point& local_location,
      blink::mojom::PluginActionType plugin_action) = 0;

  // Send a message to the RenderFrame to trigger an action on an
  // accessibility object.
  virtual void AccessibilityPerformAction(const ui::AXActionData& data) = 0;

  // This is called when the user has committed to the given find in page
  // request (e.g. by pressing enter or by clicking on the next / previous
  // result buttons). It triggers sending a native accessibility event on
  // the result object on the page, navigating assistive technology to that
  // result.
  virtual void ActivateFindInPageResultForAccessibility(int request_id) = 0;

  // See RenderWidgetHost::InsertVisualStateCallback().
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
  virtual RenderViewHost* GetRenderViewHost() const = 0;

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

  // Returns whether the IP address of the last commit was publicly routable.
  virtual bool IsLastCommitIPAddressPubliclyRoutable() const = 0;

  // Returns whether the RenderFrame in the renderer process has been created
  // and still has a connection.  This is valid for all frames.
  // RenderFrameDeleted notification will later be dispatched for this frame.
  virtual bool IsRenderFrameLive() = 0;

  // Defines different states the RenderFrameHost can be in during its lifetime,
  // i.e., from the point of creation to deletion. Please see comments in
  // RenderFrameHostImpl::LifecycleStateImpl for more details.
  //
  // Compared to the internal LifecycleStateImpl, this public LifecycleState has
  // two main differences. First, it collapses kRunningUnloadHandlers and
  // kReadyToBeDeleted into a single kPendingDeletion state, since embedders
  // need not care about the difference between having started and having
  // finished running unload handlers. Second, it intentionally does not expose
  // speculative RenderFrameHosts (corresponding to the kSpeculative internal
  // state): this is a content-internal implementation detail that is planned to
  // be eventually removed, and //content embedders shouldn't rely on their
  // existence.
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  //   org.chromium.content_public.browser)
  enum class LifecycleState {
    // RenderFrameHost is waiting for an acknowledgment from the renderer to
    // to commit a cross-RenderFrameHost navigation and swap in this
    // RenderFrameHost. Documents are in this state from
    // WebContentsObserver::ReadyToCommitNavigation to
    // WebContentsObserver::DidFinishNavigation.
    kPendingCommit,

    // RenderFrameHost committed in a primary page.
    // Documents in this state are visible to the user. kActive is the most
    // common case and the documents that have reached DidFinishNavigation will
    // be in this state (except for prerendered documents). A RenderFrameHost
    // can also be created in this state for an initial empty document when
    // creating new root frames or new child frames on a primary page.
    //
    // With MPArch (crbug.com/1164280), a WebContents may have multiple
    // coexisting pages (trees of documents), including a primary page
    // (currently shown to the user), prerendered pages, and/or pages in
    // BackForwardCache, where the two latter kinds of pages may become primary.
    kActive,

    // Prerender2:
    // RenderFrameHost committed in a prerendered page.
    // A RenderFrameHost can reach this state after a navigation in a
    // prerendered page, or be created in this state for an initial empty
    // document when creating new root frames or new child frames on a
    // prerendered page.
    //
    // Documents in this state are invisible to the user and aren't allowed to
    // show any UI changes, but the page is allowed to load and run in the
    // background. Documents in kPrerendering state can be evicted
    // (canceling prerendering) at any time (e.g. by calling
    // IsInactiveAndDisallowActivation).
    kPrerendering,

    // RenderFrameHost is stored in BackForwardCache.
    // A document may be stored in BackForwardCache after the user has navigated
    // away so that the RenderFrameHost can be re-used after history navigation.
    kInBackForwardCache,

    // RenderFrameHost is waiting to be unloaded and deleted, and is no longer
    // visible to the user.
    // After a cross-document navigation, the old documents are going to run
    // unload handlers in the background and will be deleted thereafter e.g.
    // after a DidFinishNavigation in the same frame for a different
    // RenderFrameHost, up until RenderFrameDeleted. Use
    // RenderFrameHostWrapper::WaitUntilRenderFrameDelete() to wait until
    // RenderFrameHost is deleted in tests.
    kPendingDeletion,
  };

  // Returns the LifecycleState associated with this RenderFrameHost.
  // Features that display UI to the user (or cross document/tab boundary in
  // general, e.g. when using WebContents::FromRenderFrameHost) should first
  // check whether the RenderFrameHost is in the appropriate lifecycle state.
  //
  // TODO(crbug.com/40171294): Currently, //content embedders that
  // observe WebContentsObserver::RenderFrameCreated() may also learn about
  // speculative RenderFrameHosts, which is the state before a RenderFrameHost
  // becomes kPendingCommit and is picked as the final RenderFrameHost for a
  // navigation.  The speculative state is a content-internal implementation
  // detail that may go away and should not be relied on, and hence
  // GetLifecycleState() will crash if it is called on a RenderFrameHost in such
  // a state.  Eventually, we should make sure that embedders only learn about
  // new RenderFrameHosts when they reach the kPendingCommit state.
  // If you want to use GetLifecycleState to check the state for speculative
  // RenderFrameHosts, use RenderFrameHost::IsInLifecycleState to avoid crashing
  // for speculative RenderFrameHosts.
  virtual LifecycleState GetLifecycleState() = 0;

  // Returns true if and only if the `lifecycle_state` matches
  // `GetLifecycleState`. This is helpful for determining if a RenderFrameHost
  // is in a specific state since GetLifecycleState can crash on speculative
  // frames. TODO(crbug.com/40171294): Remove this method once
  // GetLifecycleState() can be used for speculative.
  virtual bool IsInLifecycleState(LifecycleState lifecycle_state) = 0;

  // Returns true if the document hosted in this RenderFrameHost is committed
  // and lives inside a page presented to the user for the WebContents it is in
  // (e.g., not a prerendered or back-forward cached page). Only active RFHs
  // should show UI elements (e.g., prompts, color picker, etc) to the user, so
  // this method should be checked before showing some UI on behalf of a given
  // RenderFrameHost (in particular, inside handlers for IPCs from a renderer
  // process) or when crossing document/tab boundary in general, e.g., when
  // using WebContents::FromRenderFrameHost.
  //
  // IsActive() is generally the same as GetLifecycleState() == kActive, except
  // during a small window in RenderFrameHostManager::CommitPending which
  // happens before updating the next LifecycleState of old RenderFrameHost. Due
  // to this, IsActive() is preferred instead of using LifecycleState::kActive.
  // TODO(crbug.com/40168690): Make IsActive and GetLifecycleState() == kActive
  // always match.
  virtual bool IsActive() const = 0;

  // Checks that the RenderFrameHost is inactive (with some exceptions) and
  // ensures that it will be never activated if it is inactive when calling this
  // function.
  //
  // Side effect: In the case of the RenderFrameHost is inactive, this ensures
  // it will be never activated through the following:
  //
  // - For BackForwardCache: it evicts the document from the cache and
  //   triggers deletion.
  // - For Prerendering: it cancels prerendering and triggers deletion.
  //
  // This should be used when we are unsure if inactive RenderFrameHosts can
  // properly handle events and events processing shouldn't or can't be deferred
  // until the RenderFrameHost becomes active again. This allows the callers to
  // safely ignore the event as the RenderFrameHost will never be shown to the
  // user again.
  //
  // This should not be used just to check whether a RenderFrameHost is active
  // or not. For that, use |IsActive()| instead.
  //
  // This should not be used for speculative and pending commit
  // RenderFrameHosts as disallowing activation is not supported. In that case
  // |IsInactiveAndDisallowActivation()| returns false along with terminating
  // the renderer process.
  //
  // Return value: The opposite of |IsActive()|, except in some uncommon cases:
  //
  // - The "small window" referred to in the |IsActive()| documentation.
  // - For speculative and pending commit RenderFrameHosts, as mentioned above.
  //
  // |reason| will be logged via UMA and UKM. It is recommended to provide
  // a unique value for each caller.
  //
  // Embedders should use a value equal to or greater than
  // DisallowActivationReasonId.kMinEmbedderDisallowActivationReason.
  virtual bool IsInactiveAndDisallowActivation(uint64_t reason) = 0;

  // Get the number of proxies to this frame, in all processes. Exposed for
  // use by resource metrics.
  virtual size_t GetProxyCount() = 0;

  // Returns the Page associated with this RenderFrameHost. Both GetPage() and
  // GetMainFrame()->GetPage() will always return the same value.
  //
  // NOTE: For now, the associated Page object might change (when a navigation
  // is reusing RenderFrameHost and a new document is created in this
  // RenderFrameHost). The removal of this case is tracked in crbug.com/936696.
  virtual Page& GetPage() = 0;

  // Returns true if the frame has a selection.
  virtual bool HasSelection() = 0;

  // Text surrounding selection.
  virtual void RequestTextSurroundingSelection(
      base::OnceCallback<void(const std::u16string&, uint32_t, uint32_t)>
          callback,
      int max_length) = 0;

  // Generates an intervention report in this frame.
  virtual void SendInterventionReport(const std::string& id,
                                      const std::string& message) = 0;

  // Returns the WebUI object associated wit this RenderFrameHost or nullptr
  // otherwise.
  virtual WebUI* GetWebUI() = 0;

  // Tell the render frame to enable a set of javascript bindings.
  virtual void AllowBindings(BindingsPolicySet bindings) = 0;

  // Returns the set of bindings types that have been enabled for this
  // RenderFrame.
  virtual BindingsPolicySet GetEnabledBindings() = 0;

  // Sets a property with the given name and value on the WebUI object
  // associated with this RenderFrameHost, if one exists.
  virtual void SetWebUIProperty(const std::string& name,
                                const std::string& value) = 0;

#if BUILDFLAG(IS_ANDROID)
  // Returns the Java object of this instance.
  virtual jni_zero::ScopedJavaLocalRef<jobject> GetJavaRenderFrameHost() = 0;

  // Returns an InterfaceProvider for Java-implemented interfaces that are
  // scoped to this RenderFrameHost. This provides access to interfaces
  // implemented in Java in the browser process to C++ code in the browser
  // process.
  virtual service_manager::InterfaceProvider* GetJavaInterfaces() = 0;
#endif  // BUILDFLAG(IS_ANDROID)

  // Stops and disables the hang monitor for beforeunload. This avoids flakiness
  // in tests that need to observe beforeunload dialogs, which could fail if the
  // timeout skips the dialog.
  virtual void DisableBeforeUnloadHangMonitorForTesting() = 0;
  virtual bool IsBeforeUnloadHangMonitorDisabledForTesting() = 0;

  // Check whether the specific Blink feature is currently preventing fast
  // shutdown of the frame.
  virtual bool GetSuddenTerminationDisablerState(
      blink::mojom::SuddenTerminationDisablerType disabler_type) = 0;

  // Returns the permission policy for this frame.
  virtual const blink::PermissionsPolicy* GetPermissionsPolicy() = 0;

  // Returns the parsed permissions policy header for this frame.
  virtual const blink::ParsedPermissionsPolicy&
  GetPermissionsPolicyHeader() = 0;

  // Returns true if the queried PermissionsPolicyFeature is allowed by
  // permissions policy.
  virtual bool IsFeatureEnabled(
      blink::mojom::PermissionsPolicyFeature feature) = 0;

  // Opens view-source tab for the document last committed in this
  // RenderFrameHost.
  virtual void ViewSource() = 0;

  // Run the given action on the media player location at the given point.
  virtual void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point& location,
      const blink::mojom::MediaPlayerAction& action) = 0;

  // Requests the current video frame and bounds of the media player at
  // `location`. The returned image is scaled if needed to be bounded by
  // `max_size` with aspect ratio preserved, unless the original area is already
  // less than `max_area`, where `max_size` and `max_area` are both in
  // device-independent pixels. This is to avoid scaling images with very
  // large/small aspect ratio to avoid losing information. If any of the
  // dimensions is non-positive, no scaling will be performed. The bounds
  // originate from the DOM layer, are in DIP and are relative to the local
  // root's widget (see Element::BoundsInWidget()). No guarantee is made about
  // their correlation with the bounds of the video frame as displayed in the
  // presentation layer. The returned bounds are also not guaranteed to
  // correspond to the result of returned video frame.
  virtual void RequestVideoFrameAtWithBoundsHint(
      const gfx::Point& location,
      const gfx::Size& max_size,
      int max_area,
      base::OnceCallback<void(const SkBitmap&, const gfx::Rect&)> callback) = 0;

  // Creates a Network Service-backed factory from appropriate |NetworkContext|.
  //
  // If this returns true, any redirect safety checks should be bypassed in
  // downstream loaders.  (This indicates that a layer above //content has
  // wrapped `default_factory_receiver` and may inject arbitrary redirects - for
  // example see WebRequestAPI::MaybeProxyURLLoaderFactory.)
  //
  // The parameters of the new URLLoaderFactory will be based on the current
  // state of `this` RenderFrameHost.  For example, the
  // `request_initiator_origin_lock` parameter will be based on the last
  // committed origin (or on the origin of the initial empty document if one is
  // currently hosted in the frame).
  virtual bool CreateNetworkServiceDefaultFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          default_factory_receiver) = 0;

  // Requests that future URLLoaderFactoryBundle(s) sent to the renderer should
  // use a separate URLLoaderFactory for requests initiated by isolated worlds
  // listed in |isolated_world_origins|.  The URLLoaderFactory(s) for each
  // origin will be created via
  // ContentBrowserClient::CreateURLLoaderFactoryForNetworkRequests method.
  virtual void MarkIsolatedWorldsAsRequiringSeparateURLLoaderFactory(
      const base::flat_set<url::Origin>& isolated_world_origins,
      bool push_to_renderer_now) = 0;

  // Returns true if the given sandbox flag |flags| is in effect on this frame.
  // The effective flags include those which have been set by a
  // Content-Security-Policy header, in addition to those which are set by the
  // embedding frame.
  virtual bool IsSandboxed(network::mojom::WebSandboxFlags flags) = 0;

  // Calls |FlushForTesting()| on Network Service and FrameNavigationControl
  // related interfaces to make sure all in-flight mojo messages have been
  // received by the other end. For test use only.
  //
  // It is usually an error to call this method when the frame doesn't have any
  // NetworkService connection.  OTOH, tests that can't easily tell when this
  // may happen can set `do_nothing_if_no_network_service_connection` to true
  // (this should be needed relatively rarely).
  virtual void FlushNetworkAndNavigationInterfacesForTesting(
      bool do_nothing_if_no_network_service_connection) = 0;

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
  //  1- Dispatch beforeunload event on this frame and all of the frame's
  //     subframes, and wait for all beforeunload events to complete.
  //  2- Will create and return a new RenderFrameHost (destroying this one) if
  //     this RenderFrameHost is a cross-process subframe. (Note: This might not
  //     be needed anymore now that MimeHandlerView's embedded case uses the
  //     same code path as the full page case. See https://crbug.com/1398111).
  // After steps 1-2 are completed, the callback is invoked asynchronously with
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

  // Returns the type of frame owner element for the FrameTreeNode associated
  // with this RenderFrameHost (e.g., <iframe>, <object>, etc). Note that it
  // returns blink::FrameOwnerElementType::kNone if the RenderFrameHost
  // is a main frame.
  virtual blink::FrameOwnerElementType GetFrameOwnerElementType() = 0;

  // Returns the transient bit of the User Activation v2 state of the
  // FrameTreeNode associated with this RenderFrameHost.
  virtual bool HasTransientUserActivation() = 0;

  // Notifies the renderer of a user activation event for the associated frame.
  // The |notification_type| parameter is used for histograms only.
  virtual void NotifyUserActivation(
      blink::mojom::UserActivationNotificationType notification_type) = 0;

  // Reloads the frame. It initiates a reload but doesn't wait for it to finish.
  // In some rare cases, there is no history related to the frame, nothing
  // happens and this returns false.
  virtual bool Reload() = 0;

  // Returns true if this frame has fired DOMContentLoaded.
  virtual bool IsDOMContentLoaded() = 0;

  // Update whether the frame is considered an ad frame by Ad Tagging.
  //
  // Note: This ad status is currently maintained and updated *outside* content.
  // This is used to ensure the render frame proxies are in sync (since they
  // aren't exposed in the public API). Eventually, we might be able to simplify
  // this somewhat (maybe //content would be responsible for maintaining the
  // state, with some content client method used to update it).
  virtual void UpdateIsAdFrame(bool is_ad_frame) = 0;

  // Tells the host that this is part of setting up a WebXR DOM Overlay. This
  // starts a short timer that permits entering fullscreen mode, similar to a
  // recent orientation change.
  virtual void SetIsXrOverlaySetup() = 0;

  // Returns the UKM source id for the page load (last committed cross-document
  // non-bfcache navigation in the outermost main frame).
  // This id typically has an associated PageLoad UKM event.
  // Note: this can be called on any frame, but this id for all subframes or
  // fenced frames is the same as the id for the outermost main frame.
  // Should not be called while prerendering as our data collection policy
  // disallow recording UKMs until the page activation.
  // See //content/browser/preloading/prerender/README.md#ukm-source-ids for
  // more details to record UKMS for prerendering.
  virtual ukm::SourceId GetPageUkmSourceId() = 0;

  // Report an inspector issue to devtools. Note that the issue is stored on the
  // browser-side, and may contain information that we don't want to share
  // with the renderer.
  // TODO(crbug.com/40134294): This reporting should be done directly in the
  // chrome layer in the future.
  virtual void ReportInspectorIssue(blink::mojom::InspectorIssueInfoPtr) = 0;

  // Returns whether a document uses WebOTP. Returns true if a WebOTPService is
  // created on the document.
  virtual bool DocumentUsedWebOTP() = 0;

  using TraceProto = perfetto::protos::pbzero::RenderFrameHost;
  // Write a description of this RenderFrameHost into the provided |context|.
  virtual void WriteIntoTrace(
      perfetto::TracedProto<TraceProto> context) const = 0;

  // Start/stop event log output from WebRTC on this RFH for the peer connection
  // identified locally within the RFH using the ID `lid`.
  virtual void EnableWebRtcEventLogOutput(int lid, int output_period_ms) = 0;
  virtual void DisableWebRtcEventLogOutput(int lid) = 0;

  // Return true if onload has been executed in the renderer in the main frame.
  virtual bool IsDocumentOnLoadCompletedInMainFrame() = 0;

  // Returns the raw list of favicon candidates as reported to observers via
  // since the last navigation start. If called on a subframe, returns the
  // value from the corresponding main frame.
  virtual const std::vector<blink::mojom::FaviconURLPtr>& FaviconURLs() = 0;

  // Fetch the link-rel canonical URL from the renderer process. This is used
  // for sharing to external applications. Note that this URL is validated only
  // to contain HTTP(s) URLs, but may be cross-origin. Should not be considered
  // trustworthy.
  virtual void GetCanonicalUrl(
      base::OnceCallback<void(const std::optional<GURL>&)> callback) = 0;

  // Fetch the OpenGraph metadata from the renderer process. The returned data
  // has only been validated as follows:
  // * Contained URLs are web schemes, not other schemes
  // Any other properties you want, you'll need to check yourself.
  virtual void GetOpenGraphMetadata(
      base::OnceCallback<void(blink::mojom::OpenGraphMetadataPtr)>) = 0;

  // Returns true if the last navigation in this RenderFrameHost has committed
  // an error document that is a placeholder document installed when the
  // navigation failed or was blocked, containing an error message like "This
  // site can’t be reached".
  // This can't be called for pending commit RFH because the value is set
  // during call to RenderFrameHostImpl::DidNavigate which happens after commit.
  virtual bool IsErrorDocument() const = 0;

  // Return checked and weak references, respectively, to the current document
  // in this RenderFrameHost, which will be no longer valid once the
  // RenderFrameHost is deleted or navigates to another document.
  virtual DocumentRef GetDocumentRef() = 0;
  virtual WeakDocumentPtr GetWeakDocumentPtr() = 0;

  // Enable Mojo JavaScript bindings in the renderer process. It will be
  // effective on the first creation of script context after the call is made.
  // If called at frame creation time (RenderFrameCreated) or just before a
  // document is committed (ReadyToCommitNavigation), the resulting document
  // will have the JS bindings enabled.
  //
  // If |features| is nullptr, only MojoJs will be enabled. Otherwise |features|
  // enables a set of additional features that can be used with MojoJs. For
  // example, helper methods for MojoJs to better work with Web API objects.
  virtual void EnableMojoJsBindings(mojom::ExtraMojoJsFeaturesPtr features) = 0;

  // Whether the current document is loaded inside iframe credentialless.
  // Updated on every cross-document navigation.
  virtual bool IsCredentialless() const = 0;

  // Whether the last cross-document committed navigation was initiated from the
  // browser (e.g. typing on the location bar) or from the renderer while having
  // transient user activation
  virtual bool IsLastCrossDocumentNavigationStartedByUser() const = 0;

  // Returns NavigationHandles to pending-commit cross-document navigations.
  // These navigations occur when the final RenderFrameHost for the navigation
  // has been picked, the NavigationHandle ownership has been transferred
  // to it, the CommitNavigation IPC has been sent to the renderer, and the
  // navigation is waiting for the DidCommitNavigation acknowledgement. For
  // more information on how these cross-document navigations are determined,
  // refer to LifecycleState::kPendingCommit.
  virtual std::vector<base::SafeRef<NavigationHandle>>
  GetPendingCommitCrossDocumentNavigations() const = 0;

  // Checks Blink runtime-enabled features (BREF) to create and return
  // a CookieSettingOverrides pertaining to the last committed document in the
  // frame. Can only be called on a frame with a committed navigation.
  virtual net::CookieSettingOverrides GetCookieSettingOverrides() = 0;

  // Whether a same-site navigation that happens when this RenderFrameHost is
  // the current RenderFrameHost should initiate a RenderFrameHost change, due
  // to RenderDocument. the result may differ depending on whether the
  // RenderFrameHost is a main/local root/non-local-root frame, whether it has
  // committed any navigations or not, and whether it's a crashed frame that
  // must be replaced or not.
  virtual bool ShouldChangeRenderFrameHostOnSameSiteNavigation() const = 0;

  // The embedder calls this method when a prediction model believes that the
  // user is likely to click on an anchor element and wants to report the
  // likelihood of the click. The `score` is the probability that a user will
  // click on the `url`, and it is a value between 0 and 1.
  virtual void OnPreloadingHeuristicsModelDone(const GURL& url,
                                               float score) = 0;

  // Checks if `seqno` is known to have originated from this RFH. This will only
  // return true if `seqno` represents the last clipboard write made by all
  // RFHs.
  virtual bool IsClipboardOwner(
      ui::ClipboardSequenceNumberToken seqno) const = 0;

  // Marks `seqno` as originating from this RFH.
  virtual void MarkClipboardOwner(ui::ClipboardSequenceNumberToken seqno) = 0;

  // Returns true if RenderFrameHostImpl has non-null PolicyContainerHost.
  // TODO(crbug.com/346386726): Delete this method once we have solidified the
  //   lifetime expectations of the PolicyContainerHost object.
  virtual bool HasPolicyContainerHost() const = 0;

 private:
  // This interface should only be implemented inside content.
  friend class RenderFrameHostImpl;
  RenderFrameHost() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_FRAME_HOST_H_
