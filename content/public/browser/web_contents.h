// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safety_checks.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/input/browser_controls_state.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/mhtml_generation_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_trigger_type.h"
#include "content/public/browser/prerender_handle.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/stop_find_action.h"
#include "net/base/network_handle.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom-forward.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom-forward.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-forward.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/color/color_provider_key.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/jni_zero/jni_zero.h"
#endif

namespace base {
class FilePath;
}  // namespace base

namespace blink {
namespace web_pref {
struct WebPreferences;
}
class WebInputEvent;
struct UserAgentOverride;
struct RendererPreferences;
}  // namespace blink

namespace cc {
struct BrowserControlsOffsetTagsInfo;
}  // namespace cc

namespace device {
namespace mojom {
class WakeLockContext;
}
}  // namespace device

namespace net {
struct LoadStateWithParam;
}

namespace service_manager {
class InterfaceProvider;
}

namespace ui {
struct AXPropertyFilter;
struct AXTreeUpdate;
class AXNode;
class ColorProvider;
class ColorProviderSource;
}  // namespace ui

namespace content {

class BackForwardTransitionAnimationManager;
class BrowserContext;
class BrowserPluginGuestDelegate;
class RenderFrameHost;
class RenderViewHost;
class RenderWidgetHostView;
class ScreenOrientationDelegate;
class SiteInstance;
class WebContentsDelegate;
class WebUI;
struct DropData;
struct MHTMLGenerationParams;
class PreloadingAttempt;

// WebContents is the core class in content/. A WebContents renders web content
// (usually HTML) in a rectangular area.
//
// Instantiating one is simple:
//   std::unique_ptr<content::WebContents> web_contents(
//       content::WebContents::Create(
//           content::WebContents::CreateParams(browser_context)));
//   gfx::NativeView view = web_contents->GetNativeView();
//   // |view| is an HWND, NSView*, etc.; insert it into the view hierarchy
//   // wherever it needs to go.
//
// That's it; go to your kitchen, grab a scone, and chill. WebContents will do
// all the multi-process stuff behind the scenes. More details are at
// https://www.chromium.org/developers/design-documents/multi-process-architecture
// .
//
// The owner of `std::unique_ptr<content::WebContents> web_contents` is
// responsible for ensuring that `web_contents` are destroyed (e.g. closed)
// *before* the corresponding `browser_context` is destroyed.
//
// Each WebContents has a `NavigationController`, which can be obtained from
// `GetController()`, and is used to load URLs into the WebContents, navigate
// it backwards/forwards, etc.
// See navigation_controller.h for more details.
class WebContents : public PageNavigator, public base::SupportsUserData {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  struct CONTENT_EXPORT CreateParams {
    explicit CreateParams(
        BrowserContext* context,
        base::Location creator_location = base::Location::Current());
    CreateParams(BrowserContext* context,
                 scoped_refptr<SiteInstance> site,
                 base::Location creator_location = base::Location::Current());
    CreateParams(const CreateParams& other);
    ~CreateParams();

    raw_ptr<BrowserContext> browser_context;

    // Specifying a SiteInstance here is optional.  It can be set to avoid an
    // extra process swap if the first navigation is expected to require a
    // privileged process.
    scoped_refptr<SiteInstance> site_instance;

    // The process id of the frame initiating the open.
    int opener_render_process_id = content::ChildProcessHost::kInvalidUniqueID;

    // The routing id of the frame initiating the open.
    int opener_render_frame_id = MSG_ROUTING_NONE;

    // If the opener is suppressed, then the new WebContents doesn't hold a
    // reference to its opener.
    bool opener_suppressed = false;

    // Indicates whether this WebContents was created by another window.
    // This is used when determining whether the WebContents is allowed to be
    // closed via window.close(). This may be true even with a null |opener|
    // (e.g., for blocked popups), or when the window is opened with "noopener".
    bool opened_by_another_window = false;

    // The name of the top-level frame of the new window. It is non-empty
    // when creating a named window (e.g. <a target="foo"> or
    // window.open('', 'bar')).
    std::string main_frame_name;

    // New window starts from the initial empty document. When created by an
    // opener, the latter can request an initial navigation attempt to be made.
    // This is the url specified in: `window.open(initial_popup_url, ...)`.
    // This is empty otherwise.
    GURL initial_popup_url;

    // True if the contents should be initially hidden.
    bool initially_hidden = false;

    // If non-null then this WebContents will be hosted by a BrowserPlugin.
    raw_ptr<BrowserPluginGuestDelegate> guest_delegate = nullptr;

    // Used to specify the location context which display the new view should
    // belong. This can be unset if not needed.
    gfx::NativeView context = gfx::NativeView();

    // Used to specify that the new WebContents creation is driven by the
    // renderer process. In this case, the renderer-side objects, such as
    // RenderFrame, have already been created on the renderer side, and
    // WebContents construction should take this into account.
    bool renderer_initiated_creation = false;

    // Used to specify how far WebContents::Create can initialize a renderer
    // process.
    //
    // This is useful in two scenarios:
    // - Conserving resources - e.g. tab discarding and session restore do not
    //   want to use an actual renderer process before the WebContents are
    //   loaded or reloaded.  This can be accomplished via kNoRendererProcess.
    // - Avoiding the latency of the first navigation
    //   - kInitializeAndWarmupRendererProcess is most aggressive in avoiding
    //     the latency, but may be incompatible with scenarios that require
    //     manipulating the freshly created WebContents prior to initializing
    //     renderer-side objects (e.g. in scenarios like
    //     WebContentsImpl::CreateNewWindow which needs to copy the
    //     SessionStorageNamespace)
    //   - kOkayToHaveRendererProcess is the default latency-conserving mode.
    //     In this mode a spare, pre-spawned RenderProcessHost may be claimed
    //     by the newly created WebContents, but no renderer-side objects will
    //     be initialized from within WebContents::Create method.
    //
    // Note that the pre-created renderer process may not be used if the first
    // navigation requires a dedicated or privileged process, such as a WebUI.
    // This can be avoided by ensuring that |site_instance| matches the first
    // navigation's destination.
    enum RendererInitializationState {
      // Creation of WebContents should not spawn a new OS process and should
      // not reuse a RenderProcessHost that might be associated with an existing
      // OS process (as in the case of SpareRenderProcessHostManager).
      kNoRendererProcess,

      // Created WebContents may or may not be associated with an actual OS
      // process.
      kOkayToHaveRendererProcess,

      // Ensures that the created WebContents are backed by an OS process which
      // has an initialized `blink::WebView`.
      //
      // TODO(lukasza): https://crbug.com/848366: Remove
      // kInitializeAndWarmupRendererProcess value - warming up the renderer by
      // initializing the `blink::WebView` is redundant with the warm-up that
      // can be
      // achieved by either 1) warming up the spare renderer before creating
      // WebContents and/or 2) speculative RenderFrameHost used internally
      // during a navigation.
      kInitializeAndWarmupRendererProcess,
    } desired_renderer_state = kOkayToHaveRendererProcess;

    // Sandboxing flags set on the new WebContents.
    network::mojom::WebSandboxFlags starting_sandbox_flags =
        network::mojom::WebSandboxFlags::kNone;

    // Value used to set the last time the WebContents was made active, this is
    // the value that'll be returned by GetLastActiveTimeTicks(). If this is
    // left default initialized then the value is not passed on to the
    // WebContents and GetLastActiveTimeTicks() will return the WebContents'
    // creation time.
    base::TimeTicks last_active_time_ticks;

    // Value used to set the last time the WebContents was made active, this is
    // the value that'll be returned by GetLastActiveTime(). If this is left
    // default initialized then the value is not passed on to the WebContents
    // and GetLastActiveTime() will return the WebContents' creation time.
    base::Time last_active_time;

    // Code location responsible for creating the CreateParams.  This is used
    // mostly for debugging (e.g. to help attribute specific scenarios or
    // invariant violations to a particular flavor of WebContents).
    base::Location creator_location;

#if BUILDFLAG(IS_ANDROID)
    // Same as `creator_location`, for WebContents created via Java. This
    // java.lang.Throwable contains the entire
    // WebContentsCreator.createWebContents() stack trace.
    base::android::ScopedJavaGlobalRef<jthrowable> java_creator_location;
#endif  // BUILDFLAG(IS_ANDROID)

    // Enables contents to hold wake locks, for example, to keep the screen on
    // while playing video.
    bool enable_wake_locks = true;

    // Options specific to WebContents created for picture-in-picture windows.
    std::optional<blink::mojom::PictureInPictureWindowOptions>
        picture_in_picture_options;

    // Enable preview mode that shows a page with a capability restriction
    // for previewing the page.
    bool preview_mode = false;

    // The network handle bound to a target network, is used to handle the
    // loading requests over that specific network for the WebContents to be
    // created. The value `kInvalidNetworkHandle` indicates that the current
    // default network will be used.
    net::handles::NetworkHandle target_network =
        net::handles::kInvalidNetworkHandle;
  };

  // Token that causes input to be blocked on this WebContents for at least as
  // long as it exists.
  class CONTENT_EXPORT ScopedIgnoreInputEvents {
   public:
    ~ScopedIgnoreInputEvents();

    ScopedIgnoreInputEvents(ScopedIgnoreInputEvents&&);
    ScopedIgnoreInputEvents& operator=(ScopedIgnoreInputEvents&&);

   private:
    friend class WebContentsImpl;
    explicit ScopedIgnoreInputEvents(base::OnceClosure on_destruction_cb);

    base::ScopedClosureRunner on_destruction_cb_;
  };

  // Creates a new WebContents.
  //
  // The caller is responsible for ensuring that the returned WebContents is
  // destroyed (e.g. closed) *before* the BrowserContext associated with
  // `params` is destroyed.  It is a security bug if WebContents haven't been
  // destroyed when the destructor of BrowserContext starts running.  It is not
  // necessarily a bug if WebContents haven't been destroyed when
  // BrowserContext::NotifyWillBeDestroyed starts running.
  //
  // Best practices for managing the lifetime of `WebContents` and
  // `BrowserContext` will vary across different //content embedders.  For
  // example, for information specific to the //chrome layer, please see the
  // "Managing lifetime of a Profile" section in
  // //chrome/browser/profiles/README.md.
  CONTENT_EXPORT static std::unique_ptr<WebContents> Create(
      const CreateParams& params);

  // Similar to Create() above but should be used when you need to prepopulate
  // the SessionStorageNamespaceMap of the WebContents. This can happen if
  // you duplicate a WebContents, try to reconstitute it from a saved state,
  // or when you create a new WebContents based on another one (eg., when
  // servicing a window.open() call).
  //
  // You do not want to call this. If you think you do, make sure you completely
  // understand when SessionStorageNamespace objects should be cloned, why
  // they should not be shared by multiple WebContents, and what bad things
  // can happen if you share the object.
  CONTENT_EXPORT static std::unique_ptr<WebContents> CreateWithSessionStorage(
      const CreateParams& params,
      const SessionStorageNamespaceMap& session_storage_namespace_map);

  // Returns the WebContents that owns the RenderViewHost.
  //
  // WARNING: `rvh` may belong to a prerendered page, a page in the back/forward
  // cache, or a pending deletion page, so it might be inappropriate for it to
  // to trigger changes to the WebContents. See also the below comments for
  // FromRenderFrameHost().
  CONTENT_EXPORT static WebContents* FromRenderViewHost(RenderViewHost* rvh);

  // Returns the WebContents for the RenderFrameHost. It is unsafe to call this
  // function with an invalid (e.g. destructed) `rfh`.
  //
  // WARNING: It might be inappropriate for `rfh` to trigger changes to the
  // WebContents, so be careful when calling this. Some cases to be aware of
  // are:
  // * Pages/documents which are not active are not observable by the user
  //   and therefore should not show UI elements (e.g., a colour picker). These
  //   features should use `rfh->IsActive()` to determine whether `rfh` is
  //   active. See the comments there for more information.
  // * Pages/documents which are not primary generally should not update
  //   per-WebContents state (e.g., theme colour). Use
  //   `rfh->GetPage().IsPrimary()` to check for primary. Fenced frames are
  //   one case where a RenderFrameHost can be active but not primary.
  CONTENT_EXPORT static WebContents* FromRenderFrameHost(RenderFrameHost* rfh);

  // Returns the WebContents associated with the |frame_tree_node_id|. This may
  // return nullptr if the RenderFrameHost is shutting down.
  CONTENT_EXPORT static WebContents* FromFrameTreeNodeId(
      FrameTreeNodeId frame_tree_node_id);

  // A callback that returns a pointer to a WebContents. The callback can
  // always be used, but it may return nullptr: if the info used to
  // instantiate the callback can no longer be used to return a WebContents,
  // nullptr will be returned instead.
  // The callback should only run on the UI thread and it should always be
  // non-null.
  // Most uses of Getter and OnceGetter can likely be safety replaced with
  // base::WeakPtr<WebContents>.
  using Getter = base::RepeatingCallback<WebContents*(void)>;
  // Use this variant for instances that will only run the callback a single
  // time.
  using OnceGetter = base::OnceCallback<WebContents*(void)>;

  // Sets delegate for platform specific screen orientation functionality.
  CONTENT_EXPORT static void SetScreenOrientationDelegate(
      ScreenOrientationDelegate* delegate);

  ~WebContents() override = default;

  // Intrinsic tab state -------------------------------------------------------

  // Gets/Sets the delegate.
  virtual WebContentsDelegate* GetDelegate() = 0;
  virtual void SetDelegate(WebContentsDelegate* delegate) = 0;

  // Gets the NavigationController for primary frame tree of this WebContents.
  // See comments on NavigationController for more details.
  virtual NavigationController& GetController() = 0;

  // Returns the user browser context associated with this WebContents (via the
  // NavigationController).
  virtual content::BrowserContext* GetBrowserContext() = 0;

  // Returns a weak pointer.
  virtual base::WeakPtr<WebContents> GetWeakPtr() = 0;

  // Gets the URL that is currently being displayed, if there is one.
  // This method is deprecated. DO NOT USE! Pick either |GetVisibleURL| or
  // |GetLastCommittedURL| as appropriate.
  virtual const GURL& GetURL() = 0;

  // Gets the virtual URL currently being displayed in the URL bar, if there is
  // one. This URL might be a pending navigation that hasn't committed yet, so
  // it is not guaranteed to match the current page in this WebContents.
  virtual const GURL& GetVisibleURL() = 0;

  // Gets the virtual URL of the last committed page in this WebContents.
  // Virtual URLs are meant to be displayed to the user (e.g., they include the
  // "view-source:" prefix for view source URLs, unlike NavigationEntry::GetURL
  // and NavigationHandle::GetURL). The last committed page is the current
  // security context and the content that is actually displayed within the tab.
  // See also GetVisibleURL above, which may differ from this URL. Note that
  // this might return an empty GURL if no navigation has committed in the
  // WebContents' main frame.
  virtual const GURL& GetLastCommittedURL() = 0;

  // Returns the primary main frame for the currently active page. Always
  // non-null except during WebContents destruction. This WebContents may
  // have additional main frames for prerendered pages, bfcached pages, etc.
  // See docs/frame_trees.md for more details.
  virtual const RenderFrameHost* GetPrimaryMainFrame() const = 0;
  virtual RenderFrameHost* GetPrimaryMainFrame() = 0;

  // Returns the current page in the primary frame tree of this WebContents.
  // If this WebContents is associated with an omnibox, usually the URL of the
  // main document of this page will be displayed in it.
  //
  // Primary page can change as a result of a navigation, both to a new page
  // (navigation loading a new main document) and an existing one (when
  // restoring the page from back/forward cache or activating a prerendering
  // page). This change can be observed using
  // WebContentsObserver::PrimaryPageChanged, see the comments there for more
  // details.
  //
  // The primary page's lifetime corresponds to its main document's lifetime
  // and may differ from a RenderFrameHost's lifetime (for cross-document same
  // RenderFrameHost navigations).
  //
  // Apart from the primary page, additional pages might be associated with this
  // WebContents:
  // - Pending commit pages (which will become primary after-and-if the ongoing
  //   main frame navigation successfully commits).
  // - Pending deletion pages (pages the user has navigated from, but which are
  //   still alive as they are running unload handlers in background).
  // - Pages in back/forward cache (which can be navigated to later).
  // - Prerendered pages (pages which are loading in the background in
  //   anticipation of user navigating to them).
  //
  // Given the existence of multiple pages, in many cases (especially when
  // handling IPCs from the renderer process), calling GetPrimaryPage would not
  // be appropriate as it might return a wrong page. If the code already has a
  // reference to RenderFrameHost or a Page (e.g. each IPC from the renderer
  // process should be associated with a particular RenderFrameHost), it should
  // be used instead of getting the primary page from the WebContents.
  // See docs/frame_trees.md for more details.
  virtual Page& GetPrimaryPage() = 0;

  // Returns the focused frame for the primary page or an inner page thereof.
  // Might be nullptr if nothing is focused.
  virtual RenderFrameHost* GetFocusedFrame() = 0;

  // Returns true if |frame_tree_node_id| refers to a frame in a prerendered
  // page. TODO(https://crbug.com/40176578, https://crbug.com/40191159): This
  // will be extended to also return true if it is in an inner page of a
  // prerendered page.
  virtual bool IsPrerenderedFrame(FrameTreeNodeId frame_tree_node_id) = 0;

  // NOTE: This is generally unsafe to use. A frame's RenderFrameHost may
  // change over its lifetime, such as during cross-process navigation (and
  // thus privilege change). Use RenderFrameHost::FromID instead wherever
  // possible.
  //
  // Given a FrameTreeNodeId that belongs to this WebContents, returns the
  // current RenderFrameHost regardless of which FrameTree it is in.
  virtual RenderFrameHost* UnsafeFindFrameByFrameTreeNodeId(
      FrameTreeNodeId frame_tree_node_id) = 0;

  // Calls |on_frame| for every RenderFrameHost in this WebContents. Note that
  // this includes RenderFrameHosts that are not descended from the primary main
  // frame (e.g. bfcached pages and prerendered pages). The order of traversal
  // for RenderFrameHosts within a page is consistent with
  // |RenderFrameHost::ForEachRenderFrameHost|'s order, however no order is
  // guaranteed between pages.
  // For callers only interested in the primary page,
  // |GetMainFrame()->ForEachRenderFrameHost()| can be used.
  // See |RenderFrameHost::ForEachRenderFrameHost| for details.
  using FrameIterationAction = RenderFrameHost::FrameIterationAction;
  virtual void ForEachRenderFrameHostWithAction(
      base::FunctionRef<FrameIterationAction(RenderFrameHost*)> on_frame) = 0;
  virtual void ForEachRenderFrameHost(
      base::FunctionRef<void(RenderFrameHost*)> on_frame) = 0;

  // Gets the current RenderViewHost for this tab.
  virtual RenderViewHost* GetRenderViewHost() = 0;

  // Returns the currently active RenderWidgetHostView. This may change over
  // time and can be nullptr (during setup and teardown).
  virtual RenderWidgetHostView* GetRenderWidgetHostView() = 0;

  // Returns the outermost RenderWidgetHostView. This will return the platform
  // specific RenderWidgetHostView (as opposed to
  // RenderWidgetHostViewChildFrame), which can be used to create context
  // menus.
  virtual RenderWidgetHostView* GetTopLevelRenderWidgetHostView() = 0;

  // This determines which RenderFrameHosts will contribute to the
  // `AXTreeUpdate` generated by the `RequestAXTreeSnapshot` call:
  // - kAll will use all reachable RenderFrameHosts.
  // - kSameOriginDirectDescendants will prune the tree if a RenderFrameHost
  //   crosses an origin, site, or fencedframe boundary. Note that even if a
  //   descendant goes back to being same origin (A -> B -> A) case, the
  //   descendant will be skipped.
  enum class AXTreeSnapshotPolicy { kAll, kSameOriginDirectDescendants };

  // Request a one-time snapshot of the accessibility tree without changing the
  // accessibility mode. See RenderFrame::CreateAXTreeSnapshotter for
  // definitions of |ax_mode|, and AXTreeSnapshotter::Snapshot for definitions
  // of |max_nodes| and |timeout|. |policy| is used directly in
  // WebContentsImpl::RequestAXTreeSnapshot; its meaning is explained in the
  // AXTreeSnapshotPolicy definition above.
  using AXTreeSnapshotCallback = base::OnceCallback<void(ui::AXTreeUpdate&)>;
  virtual void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                                     ui::AXMode ax_mode,
                                     size_t max_nodes,
                                     base::TimeDelta timeout,
                                     AXTreeSnapshotPolicy policy) = 0;

  // Causes the current page to be closed, including running its onunload event
  // handler.
  virtual void ClosePage() = 0;

  // Returns the theme color for the underlying content as set by the
  // theme-color meta tag if any.
  virtual std::optional<SkColor> GetThemeColor() = 0;

  // Returns the background color for the underlying content as set by CSS if
  // any.
  virtual std::optional<SkColor> GetBackgroundColor() = 0;

  // Sets the renderer-side default background color of the page. This is used
  // when the page has not loaded enough to know a background color or if the
  // page does not set a background color.
  // Pass in nullopt to reset back to the default.
  // Note there are situations where the base background color is not used, such
  // as fullscreen.
  // Note currently this is sent directly to the renderer, so does not interact
  // directly with `RenderWidgetHostView::SetBackgroundColor`. There is pending
  // refactor to remove `RenderWidgetHostView::SetBackgroundColor` and merge its
  // functionality here, which will be more consistent and simpler to
  // understand.
  virtual void SetPageBaseBackgroundColor(std::optional<SkColor> color) = 0;

  // Sets the ColorProviderSource for the WebContents. The WebContents will
  // maintain an observation of `source` until a new source is set or the
  // current source is destroyed. WebContents will receive updates when the
  // source's ColorProvider changes.
  virtual void SetColorProviderSource(ui::ColorProviderSource* source) = 0;

  // Returns the ColorProvider instance for this WebContents object. This will
  // always return a valid ColorProvider instance.
  virtual const ui::ColorProvider& GetColorProvider() const = 0;

  // Gets the color mode for the ColorProvider associated with this WebContents.
  virtual ui::ColorProviderKey::ColorMode GetColorMode() const = 0;

  // Returns the committed WebUI if one exists.
  virtual WebUI* GetWebUI() = 0;

  // Sets the user-agent that may be used for navigations in this WebContents.
  // The user-agent is *only* used when
  // NavigationEntry::SetIsOverridingUserAgent(true) is used (the value of
  // is-overriding-user-agent may be specified in LoadURLParams). If
  // |override_in_new_tabs| is true, and the first navigation in the tab is
  // renderer initiated, then is-overriding-user-agent is set to true for the
  // NavigationEntry. See SetRendererInitiatedUserAgentOverrideOption() for
  // details on how renderer initiated navigations are configured.
  //
  // If nonempty, |ua_override|'s value must not contain '\0', '\r', or '\n' (in
  // other words, it must be a valid HTTP header value).
  virtual void SetUserAgentOverride(const blink::UserAgentOverride& ua_override,
                                    bool override_in_new_tabs) = 0;

  // Configures the value of is-overriding-user-agent for renderer initiated
  // navigations. The default is UA_OVERRIDE_INHERIT. This value does not apply
  // to the first renderer initiated navigation if the tab has no navigations.
  // See SetUserAgentOverride() for details on that.
  virtual void SetRendererInitiatedUserAgentOverrideOption(
      NavigationController::UserAgentOverrideOption option) = 0;

  virtual const blink::UserAgentOverride& GetUserAgentOverride() = 0;

  // Updates all renderers to start sending subresource notifications since a
  // certificate error or HTTP exception has been allowed by the user.
  virtual void SetAlwaysSendSubresourceNotifications() = 0;
  virtual bool GetSendSubresourceNotification() = 0;

  // Returns true only if the WebContentsObserver accessibility mode is
  // enabled.
  virtual bool IsWebContentsOnlyAccessibilityModeForTesting() = 0;

  // Returns true only if complete accessibility mode is on, meaning there's
  // both renderer accessibility, and a native browser accessibility tree.
  virtual bool IsFullAccessibilityModeForTesting() = 0;

  virtual ui::AXMode GetAccessibilityMode() = 0;

  // Forces a reset of accessibility state in the instance's renderers.
  // Observers will receive a new accessibility tree.
  virtual void ResetAccessibility() = 0;

  // Returns a pointer to the root node of the live accessibility tree for the
  // main frame, if accessibility is turned on. Otherwise, returns nullptr.
  virtual ui::AXNode* GetAccessibilityRootNode() = 0;

  virtual std::string DumpAccessibilityTree(
      bool internal,
      std::vector<ui::AXPropertyFilter> property_filters) = 0;

  virtual std::string DumpAccessibilityTree(
      ui::AXApiType::Type api_type,
      std::vector<ui::AXPropertyFilter> property_filters) = 0;

  // A callback that takes a string which contains accessibility event
  // information.
  using AccessibilityEventCallback =
      base::RepeatingCallback<void(const std::string&)>;

  // Starts or stops recording accessibility events. |start_recording| is true
  // when recording should start and false when recording should stop.
  // |callback| is an optional function which is called when an accessibility
  // event is received while accessibility events are being recorded. When
  // |start_recording| is true, it is expected that |callback| has a value; when
  // |start_recording| is false, it is expected that |callback| does not.
  virtual void RecordAccessibilityEvents(
      bool start_recording,
      std::optional<AccessibilityEventCallback> callback) = 0;

  virtual void RecordAccessibilityEvents(
      ui::AXApiType::Type api_type,
      bool start_recording,
      std::optional<AccessibilityEventCallback> callback) = 0;
  // Tab navigation state ------------------------------------------------------

  // Returns the current navigation properties, which if a navigation is
  // pending may be provisional (e.g., the navigation could result in a
  // download, in which case the URL would revert to what it was previously).
  virtual const std::u16string& GetTitle() = 0;

  // Saves the given title to the navigation entry and does associated work. It
  // will update history and the view with the new title, and also synthesize
  // titles for file URLs that have none. Thus |entry| must have a URL set.
  virtual void UpdateTitleForEntry(NavigationEntry* entry,
                                   const std::u16string& title) = 0;

  // Returns app title of the current navigation entry. The apptitle is
  // an alternative title text that can be used by app windows.
  // See
  // https://github.com/MicrosoftEdge/MSEdgeExplainers/blob/main/DocumentSubtitle/explainer.md
  virtual const std::optional<std::u16string>& GetAppTitle() = 0;

  // Returns the SiteInstance associated with the current page.
  virtual SiteInstance* GetSiteInstance() = 0;

  // Returns whether this WebContents is loading a resource.
  virtual bool IsLoading() = 0;

  // Returns the current load progress.
  virtual double GetLoadProgress() = 0;

  // Returns whether a navigation is currently in progress that should show
  // loading UI if such UI exists (progress bar, loading spinner, stop button,
  // etc.) True for different-document navigations and the navigation API's
  // intercept(). This being true implies that IsLoading() is also true.
  virtual bool ShouldShowLoadingUI() = 0;

  // Returns whether the current primary main document has reached and finished
  // executing its onload() handler. Corresponds to
  // WebContentsObserver::DocumentOnLoadCompletedInPrimaryMainFrame() and see
  // comments there for more details.
  virtual bool IsDocumentOnLoadCompletedInPrimaryMainFrame() = 0;

  // Returns whether this WebContents is waiting for a first-response for the
  // main resource of the page.
  virtual bool IsWaitingForResponse() = 0;

  // Returns whether this WebContents's primary frame tree node is navigating,
  // i.e. it has an associated NavigationRequest.
  virtual bool HasUncommittedNavigationInPrimaryMainFrame() = 0;

  // Returns the current load state and the URL associated with it.
  // The load state is only updated while IsLoading() is true.
  virtual const net::LoadStateWithParam& GetLoadState() = 0;
  virtual const std::u16string& GetLoadStateHost() = 0;

  // Returns the upload progress.
  virtual uint64_t GetUploadSize() = 0;
  virtual uint64_t GetUploadPosition() = 0;

  // Returns the character encoding of the page.
  virtual const std::string& GetEncoding() = 0;

  // Discards the RenderFrame. Use is guarded by kWebContentsDiscard.
  virtual void Discard() = 0;

  // Indicates that the tab was previously discarded.
  // wasDiscarded is exposed on Document after discard, see:
  // https://github.com/WICG/web-lifecycle
  // When a tab is discarded, WebContents sets was_discarded on its
  // root FrameTreeNode.
  // In addition, when a child frame is created, this bit is passed on from
  // parent to child.
  // When a navigation request is created, was_discarded is passed on to the
  // request and reset to false in FrameTreeNode.
  virtual bool WasDiscarded() = 0;
  virtual void SetWasDiscarded(bool was_discarded) = 0;

  // Notifies observers that this WebContents is about to be discarded, and
  // replaced with `new_contents`. See the comment on
  // WebContentsObserver::AboutToBeDiscarded.
  // TODO(crbug.com/347770670): Remove this once new WebContents are no
  // longer created during discard operations. Move remaining clients to
  // `WasDiscarded`.
  virtual void AboutToBeDiscarded(WebContents* new_contents) = 0;

  // Notifies observers that this WebContents has completed its discard
  // operation.
  virtual void NotifyWasDiscarded() = 0;

  // Internal state ------------------------------------------------------------

  // Indicates whether the WebContents is being captured (e.g., for screenshots,
  // or mirroring video and/or audio). Each IncrementCapturerCount() call must
  // be balanced with a corresponding DecrementCapturerCount() call.
  //
  // Both internal-to-content and embedders must increment the capturer count
  // while capturing to ensure "hidden rendering" optimizations are disabled.
  // For example, renderers will be configured to produce compositor frames
  // regardless of their "backgrounded" or on-screen occlusion state.
  //
  // Embedders can detect whether a WebContents is being captured (see
  // IsBeingCaptured() below) and use this, for example, to provide an
  // alternative user interface. So, developers should be careful to understand
  // the side-effects from using or changing these APIs, both upstream and
  // downstream of this API layer.
  //
  // Callers must hold onto the returned base::ScopedClosureRunner until they
  // are done capturing.
  //
  // |capture_size| is only used in the case of mirroring (i.e., screen capture
  // video); otherwise, an empty gfx::Size should be provided. This specifies
  // the capturer's target video resolution, but can be empty to mean
  // "unspecified." This becomes a temporary override to GetPreferredSize(),
  // allowing embedders to size the WebContents on-screen views for optimal
  // capture quality.
  //
  // |stay_hidden| affects the page visibility state of the renderers (i.e., a
  // web page can be made aware of whether it is actually user-visible). If
  // true, the show/hide state of the WebContents will be passed to the
  // renderers, like normal. If false, the renderers will always be told they
  // are user-visible while being captured.
  //
  // |stay_awake| will cause a WakeLock to be held which prevents system sleep.
  //
  // |is_activity| means the capture will cause the last active time to be
  // updated.
  [[nodiscard]] virtual base::ScopedClosureRunner IncrementCapturerCount(
      const gfx::Size& capture_size,
      bool stay_hidden,
      bool stay_awake,
      bool is_activity) = 0;

  // Getter for the capture handle, which allows a captured application to
  // opt-in to exposing information to its capturer(s).
  virtual const blink::mojom::CaptureHandleConfig& GetCaptureHandleConfig() = 0;

  // Returns true if audio/screenshot/video is being captured by the embedder,
  // as indicated by calls to IncrementCapturerCount().
  virtual bool IsBeingCaptured() = 0;

  // Returns true if audio/screenshot/video is being captured by the embedder
  // and renderers are being told they are always user-visible, as indicated by
  // calls to IncrementCapturerCount().
  virtual bool IsBeingVisiblyCaptured() = 0;

  // Indicates/Sets whether all audio output from this WebContents is muted.
  // This does not affect audio capture, just local/system output.
  virtual bool IsAudioMuted() = 0;
  virtual void SetAudioMuted(bool mute) = 0;

  // Returns true if the audio is currently audible.
  virtual bool IsCurrentlyAudible() = 0;

  // Indicates whether any frame in the WebContents is connected to a Bluetooth
  // Device.
  virtual bool IsConnectedToBluetoothDevice() = 0;

  // Indicates whether any frame in the WebContents is scanning for Bluetooth
  // devices.
  virtual bool IsScanningForBluetoothDevices() = 0;

  // Indicates whether any frame in the WebContents is connected to a serial
  // port.
  virtual bool IsConnectedToSerialPort() = 0;

  // Indicates whether any frame in the WebContents is connected to a HID
  // device.
  virtual bool IsConnectedToHidDevice() = 0;

  // Indicates whether any frame in the WebContents is connected to a USB
  // device.
  virtual bool IsConnectedToUsbDevice() = 0;

  // Indicates whether any frame in the WebContents has File System Access
  // handles.
  virtual bool HasFileSystemAccessHandles() = 0;

  // Indicates whether a video is in Picture-in-Picture for |this|.
  virtual bool HasPictureInPictureVideo() = 0;

  // Indicates whether a document is in Picture-in-Picture for |this|.
  virtual bool HasPictureInPictureDocument() = 0;

  // Indicates whether this tab should be considered crashed. This becomes false
  // again when the renderer process is recreated after a crash in order to
  // recreate the main frame.
  virtual bool IsCrashed() = 0;

  virtual base::TerminationStatus GetCrashedStatus() = 0;
  virtual int GetCrashedErrorCode() = 0;

  // Whether the tab is in the process of being destroyed.
  virtual bool IsBeingDestroyed() = 0;

  // Convenience method for notifying the delegate of a navigation state
  // change.
  virtual void NotifyNavigationStateChanged(InvalidateTypes changed_flags) = 0;

  // Notifies the WebContents that audio state has changed. The contents is
  // aware of all of its potential sources of audio and needs to poll them
  // directly to determine its aggregate audio state.
  virtual void OnAudioStateChanged() = 0;

  // Get/Set the last time ticks that the WebContents was made active (either
  // when it was created or shown with WasShown()). Note: GetLastActiveTimeTicks
  // and GetLastActiveTime can get desynced if the process is suspended or if
  // the clock is adjusted.
  virtual base::TimeTicks GetLastActiveTimeTicks() = 0;

  // Get/Set the last time that the WebContents was made active (either when it
  // was created or shown with WasShown()).
  // Note: GetLastActiveTimeTicks and GetLastActiveTime can get desynced if the
  // process is suspended or if the clock is adjusted.
  virtual base::Time GetLastActiveTime() = 0;

  // Invoked when the WebContents becomes shown/hidden. A hidden WebContents
  // isn't painted on the screen.
  virtual void WasShown() = 0;
  virtual void WasHidden() = 0;

  // Invoked when the WebContents becomes occluded. An occluded WebContents
  // isn't painted on the screen, except in a window switching feature (e.g.
  // Alt-Tab).
  virtual void WasOccluded() = 0;

  // Returns the visibility of the WebContents' view.
  virtual Visibility GetVisibility() = 0;

  // Sets the visibility of the WebContents' view and notifies the WebContents
  // observers about Visibility change. Call UpdateWebContentsVisibility instead
  // of WasShown() if you are setting Visibility to VISIBLE for the first time.
  // TODO(crbug.com/40911760): Make updating Visibility more robust.
  virtual void UpdateWebContentsVisibility(Visibility visibility) = 0;

  // This function checks *all* frames in this WebContents (not just the main
  // frame) and returns true if at least one frame has either a beforeunload or
  // an unload/pagehide/visibilitychange handler.
  //
  // The value of this may change over time. For example, if true and the
  // beforeunload listener is executed and allows the user to exit, then this
  // returns false.
  virtual bool NeedToFireBeforeUnloadOrUnloadEvents() = 0;

  // Runs the beforeunload handler for the main frame and all its subframes.
  // See also ClosePage in RenderViewHostImpl, which runs the unload handler.
  // If |auto_cancel| is true, and the beforeunload handler returns a non-empty
  // string (indicating the page wants to present a confirmation dialog), then
  // the beforeunload operation will automatically return with |proceed=false|
  // and no dialog will be shown to the user. This is used to interrupt a
  // potential discard without causing the dialog to appear.
  virtual void DispatchBeforeUnload(bool auto_cancel) = 0;

  // Attaches |inner_web_contents| to the container frame |render_frame_host|,
  // which must be in a FrameTree for this WebContents. This outer WebContents
  // takes ownership of |inner_web_contents|.
  // Note: |render_frame_host| will be swapped out and destroyed during the
  // process. Generally a frame same-process with its parent is the right choice
  // but ideally it should be "about:blank" to avoid problems with beforeunload.
  // To ensure sane usage of this API users first should call the async API
  // RenderFrameHost::PrepareForInnerWebContentsAttach first.
  // Note: If |is_full_page| is true, focus will be given to the inner
  // WebContents.
  virtual void AttachInnerWebContents(
      std::unique_ptr<WebContents> inner_web_contents,
      RenderFrameHost* render_frame_host,
      bool is_full_page) = 0;

  // Returns whether this WebContents is an inner WebContents for a guest.
  // Important: please avoid using this in new callsites, and use
  // GetOuterWebContents instead.
  virtual bool IsInnerWebContentsForGuest() = 0;

  // Returns the outer WebContents frame, the same frame that this WebContents
  // was attached in AttachToOuterWebContentsFrame().
  virtual RenderFrameHost* GetOuterWebContentsFrame() = 0;

  // Returns the outer WebContents of this WebContents if any.
  // Otherwise, return nullptr.
  virtual WebContents* GetOuterWebContents() = 0;

  // Returns the root WebContents of the WebContents tree. Always returns
  // non-null value.
  virtual WebContents* GetOutermostWebContents() = 0;

  // Returns a vector to the inner WebContents within this WebContents.
  virtual std::vector<WebContents*> GetInnerWebContents() = 0;

  // Returns the user-visible WebContents that is responsible for the UI
  // activity in the provided WebContents. For example, this delegate may be
  // aware that the contents is embedded in some other contents, or hosts
  // background activity on behalf of a user-visible tab which should be used to
  // display dialogs and similar affordances to the user.
  //
  // This may be distinct from the outer web contents (for example, the
  // responsible contents may logically "own" a contents but not currently embed
  // it for rendering).
  //
  // Always returns a non-null value.
  //
  // TODO(crbug.com/40939539): Consider replacing this with
  // GuestViewBase::GetTopLevelWebContents, since that is now the only case
  // where this would return a contents other than |this|.
  virtual WebContents* GetResponsibleWebContents() = 0;

  // Invoked when visible security state changes.
  virtual void DidChangeVisibleSecurityState() = 0;

  // Sends the current preferences to all renderer processes for the current
  // page.
  virtual void SyncRendererPrefs() = 0;

  // Commands ------------------------------------------------------------------

  // Stop any pending navigation.
  virtual void Stop() = 0;

  // Freezes or unfreezes the current page. A frozen page runs as few tasks as
  // possible. This cannot be called when the page is visible. If the page is
  // made visible after this is called, it is automatically unfrozen.
  virtual void SetPageFrozen(bool frozen) = 0;

  // Creates a new WebContents with the same state as this one. The returned
  // heap-allocated pointer is owned by the caller.
  virtual std::unique_ptr<WebContents> Clone() = 0;

  // Reloads the focused frame.
  virtual void ReloadFocusedFrame() = 0;

  // Editing commands ----------------------------------------------------------

  virtual void Undo() = 0;
  virtual void Redo() = 0;
  virtual void Cut() = 0;
  virtual void Copy() = 0;
  virtual void CopyToFindPboard() = 0;
  virtual void CenterSelection() = 0;
  virtual void Paste() = 0;
  virtual void PasteAndMatchStyle() = 0;
  virtual void Delete() = 0;
  virtual void SelectAll() = 0;
  virtual void CollapseSelection() = 0;
  virtual void ScrollToTopOfDocument() = 0;
  virtual void ScrollToBottomOfDocument() = 0;

  // Adjust the selection starting and ending points in the focused frame by
  // the given amounts. A negative amount moves the selection towards the
  // beginning of the document, a positive amount moves the selection towards
  // the end of the document.
  virtual void AdjustSelectionByCharacterOffset(int start_adjust,
                                                int end_adjust,
                                                bool show_selection_menu) = 0;

  // Replaces the currently selected word or a word around the cursor.
  virtual void Replace(const std::u16string& word) = 0;

  // Replaces the misspelling in the current selection.
  virtual void ReplaceMisspelling(const std::u16string& word) = 0;

  // Let the renderer know that the menu has been closed.
  virtual void NotifyContextMenuClosed(const GURL& link_followed) = 0;

  // Executes custom context menu action that was provided from Blink.
  virtual void ExecuteCustomContextMenuCommand(int action,
                                               const GURL& link_followed) = 0;

  // Views and focus -----------------------------------------------------------

  // Returns the native widget that contains the contents of the tab.
  virtual gfx::NativeView GetNativeView() = 0;

  // Returns the native widget with the main content of the tab (i.e. the main
  // render view host, though there may be many popups in the tab as children of
  // the container).
  virtual gfx::NativeView GetContentNativeView() = 0;

  // Returns the outermost native view. This will be used as the parent for
  // dialog boxes.
  virtual gfx::NativeWindow GetTopLevelNativeWindow() = 0;

  // Computes the rectangle for the native widget that contains the contents of
  // the tab in the screen coordinate system.
  virtual gfx::Rect GetContainerBounds() = 0;

  // Get the bounds of the View in the global screen position.
  virtual gfx::Rect GetViewBounds() = 0;

  // Resize a WebContents to |new_bounds|.
  virtual void Resize(const gfx::Rect& new_bounds) = 0;

  // Get the size of a WebContents.
  virtual gfx::Size GetSize() = 0;

  // Returns the current drop data, if any.
  virtual DropData* GetDropData() = 0;

  // Sets focus to the native widget for this tab.
  virtual void Focus() = 0;

  // Sets focus to the appropriate element when the WebContents is shown the
  // first time.
  virtual void SetInitialFocus() = 0;

  // Stores the currently focused view.
  virtual void StoreFocus() = 0;

  // Restores focus to the last focus view. If StoreFocus has not yet been
  // invoked, SetInitialFocus is invoked.
  virtual void RestoreFocus() = 0;

  // Focuses the first (last if |reverse| is true) element in the page.
  // Invoked when this tab is getting the focus through tab traversal (|reverse|
  // is true when using Shift-Tab).
  virtual void FocusThroughTabTraversal(bool reverse) = 0;

  // Misc state & callbacks ----------------------------------------------------

  // Check whether we can do the saving page operation this page given its MIME
  // type.
  virtual bool IsSavable() = 0;

  // Prepare for saving the current web page to disk.
  virtual void OnSavePage() = 0;

  // Save page with the main HTML file path, the directory for saving resources,
  // and the save type: HTML only or complete web page. Returns true if the
  // saving process has been initiated successfully.
  virtual bool SavePage(const base::FilePath& main_file,
                        const base::FilePath& dir_path,
                        SavePageType save_type) = 0;

  // Saves the given frame's URL to the local filesystem. If `rfh` is provided,
  // the saving is performed in its context. For example, the associated
  // navigation isolation info will be used for making the network request.
  virtual void SaveFrame(const GURL& url,
                         const Referrer& referrer,
                         RenderFrameHost* rfh) = 0;

  // Saves the given frame's URL to the local filesystem. The headers, if
  // provided, is used to make a request to the URL rather than using cache.
  // Format of |headers| is a new line separated list of key value pairs:
  // "<key1>: <value1>\r\n<key2>: <value2>". The saving is performed in the
  // context of `rfh`. For example, the associated navigation isolation info
  // will be used for making the network request. If `is_subresource` is true,
  // the URL is assumed to correspond to a subresource loaded in the frame,
  // as opposed to the main (generally, document) resource.
  virtual void SaveFrameWithHeaders(const GURL& url,
                                    const Referrer& referrer,
                                    const std::string& headers,
                                    const std::u16string& suggested_filename,
                                    RenderFrameHost* rfh,
                                    bool is_subresource) = 0;

  // Generate an MHTML representation of the current page conforming to the
  // settings provided by |params| and returning final status information via
  // the callback. See MHTMLGenerationParams for details on generation settings.
  // A resulting |file_size| of -1 represents a failure. Any other value
  // represents the size of the successfully generated file.
  //
  // TODO(crbug.com/40606905): GenerateMHTML will eventually be removed
  // and GenerateMHTMLWithResult will be renamed to GenerateMHTML to replace it.
  // Both GenerateMHTML and GenerateMHTMLWithResult perform the same operation.
  // however, GenerateMHTMLWithResult provides a struct as output, that contains
  // the file size and more.
  virtual void GenerateMHTML(
      const MHTMLGenerationParams& params,
      base::OnceCallback<void(int64_t /* file_size */)> callback) = 0;
  virtual void GenerateMHTMLWithResult(
      const MHTMLGenerationParams& params,
      MHTMLGenerationResult::GenerateMHTMLCallback callback) = 0;

  // Returns the MIME type bound to the primary page contents after a primary
  // page navigation.
  virtual const std::string& GetContentsMimeType() = 0;

  // Returns the settings which get passed to the renderer.
  virtual blink::RendererPreferences* GetMutableRendererPrefs() = 0;

  // Tells the tab to close now. The tab will take care not to close until it's
  // out of nested run loops.
  virtual void Close() = 0;

  // Indicates if this tab was explicitly closed by the user (control-w, close
  // tab menu item...). This is false for actions that indirectly close the tab,
  // such as closing the window.  The setter is maintained by TabStripModel, and
  // the getter only useful from within TAB_CLOSED notification
  virtual void SetClosedByUserGesture(bool value) = 0;
  virtual bool GetClosedByUserGesture() = 0;

  // Gets the minimum/maximum zoom percent.
  virtual int GetMinimumZoomPercent() = 0;
  virtual int GetMaximumZoomPercent() = 0;

  // Set the renderer's page scale to the given factor.
  virtual void SetPageScale(float page_scale_factor) = 0;

  // Gets the preferred size of the contents.
  virtual gfx::Size GetPreferredSize() = 0;

  // Called when the response to a pending pointer lock request has arrived.
  // Returns true if |allowed| is true and the mouse has been successfully
  // locked.
  virtual bool GotResponseToPointerLockRequest(
      blink::mojom::PointerLockResult result) = 0;

  // Wrapper around GotResponseToPointerLockRequest to fit into
  // ChromeWebViewPermissionHelperDelegate's structure.
  virtual void GotPointerLockPermissionResponse(bool allowed) = 0;

  // Drop the mouse lock if it is currently locked, or reject an
  // outstanding request if it is pending.
  virtual void DropPointerLockForTesting() = 0;

  // Called when the response to a keyboard mouse lock request has arrived.
  // Returns false if the request is no longer valid, otherwise true.
  virtual bool GotResponseToKeyboardLockRequest(bool allowed) = 0;

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Called when the user has selected a color in the color chooser.
  virtual void DidChooseColorInColorChooser(SkColor color) = 0;

  // Called when the color chooser has ended.
  virtual void DidEndColorChooser() = 0;
#endif

  // Returns true if the location bar should be focused by default rather than
  // the page contents. The view calls this function when the tab is focused
  // to see what it should do.
  virtual bool FocusLocationBarByDefault() = 0;

  // Does this have an opener (corresponding to window.opener in JavaScript)
  // associated with it?
  virtual bool HasOpener() = 0;

  // Returns the opener if HasOpener() is true, or nullptr otherwise.
  virtual RenderFrameHost* GetOpener() = 0;

  // Returns true if this WebContents was opened by another WebContents, even
  // if the opener was suppressed. In contrast to HasOpener/GetOpener, the
  // "original opener chain" doesn't reflect window.opener which can be
  // suppressed or updated. The "original opener" is the main frame of the
  // actual opener of this frame. This traces the all the way back, so if the
  // original opener was closed (deleted or severed due to COOP), but _it_ had
  // an original opener, this will return the original opener's original opener,
  // etc.
  virtual bool HasLiveOriginalOpenerChain() = 0;

  // Returns the "original opener WebContents" if HasLiveOriginalOpenerChain()
  // is true, or nullptr otherwise. See the comment for
  // `HasLiveOriginalOpenerChain()` for more details.
  virtual WebContents* GetFirstWebContentsInLiveOriginalOpenerChain() = 0;

  // Returns the WakeLockContext accociated with this WebContents.
  virtual device::mojom::WakeLockContext* GetWakeLockContext() = 0;

  // |http_status_code| can be 0 e.g. for data: URLs.
  // |bitmaps| will be empty on download failure.
  // |sizes| are the sizes in pixels of the bitmaps before they were resized due
  // to the max bitmap size passed to DownloadImage(). Each entry in the bitmaps
  // vector corresponds to an entry in the sizes vector (both vector sizes are
  // guaranteed to be equal). If a bitmap was resized, there should be a single
  // returned bitmap.
  using ImageDownloadCallback =
      base::OnceCallback<void(int id,
                              int http_status_code,
                              const GURL& image_url,
                              const std::vector<SkBitmap>& bitmaps,
                              const std::vector<gfx::Size>& sizes)>;

  // Sends a request to download the given image |url| and returns the unique
  // id of the download request. When the download is finished, |callback| will
  // be called with the bitmaps received from the renderer.
  // If |is_favicon| is true, the cookies are not sent and not accepted during
  // download.
  // If there are no bitmap results <= |max_bitmap_size|, the smallest bitmap
  // is resized to |max_bitmap_size| and is the only result.
  // A |max_bitmap_size| of 0 means unlimited.
  // For vector images, |preferred_size| will serve as a viewport into which
  // the image will be rendered. This would usually be the dimensions of the
  // rectangle where the bitmap will be rendered. If |preferred_size| is empty,
  // any existing intrinsic dimensions of the image will be used. If
  // |max_bitmap_size| is non-zero it will also impose an upper bound on the
  // longest edge of |preferred_size| (|preferred_size| will be scaled down).
  // If |bypass_cache| is true, |url| is requested from the server even if it
  // is present in the browser cache.
  virtual int DownloadImage(const GURL& url,
                            bool is_favicon,
                            const gfx::Size& preferred_size,
                            uint32_t max_bitmap_size,
                            bool bypass_cache,
                            ImageDownloadCallback callback) = 0;

  // Same as DownloadImage(), but uses an ax node id to retrieve the URL once
  // the download call has reached the renderer.
  virtual int DownloadImageFromAxNode(const ui::AXTreeID tree_id,
                                      const ui::AXNodeID node_id,
                                      const gfx::Size& preferred_size,
                                      uint32_t max_bitmap_size,
                                      bool bypass_cache,
                                      ImageDownloadCallback callback) = 0;

  // Same as DownloadImage(), but uses the ImageDownloader from the
  // specified frame instead of the main frame.
  virtual int DownloadImageInFrame(
      const GlobalRenderFrameHostId& initiator_frame_routing_id,
      const GURL& url,
      bool is_favicon,
      const gfx::Size& preferred_size,
      uint32_t max_bitmap_size,
      bool bypass_cache,
      ImageDownloadCallback callback) = 0;

  // Finds text on a page. |search_text| should not be empty. |skip_delay|
  // indicates that the find request should be sent to the renderer immediately
  // instead of waiting for privacy/performance mitigations.
  virtual void Find(int request_id,
                    const std::u16string& search_text,
                    blink::mojom::FindOptionsPtr options,
                    bool skip_delay) = 0;

  // Notifies the renderer that the user has closed the FindInPage window
  // (and what action to take regarding the selection).
  virtual void StopFinding(StopFindAction action) = 0;

  // Returns true if audio has been audible from the WebContents since the last
  // navigation.
  virtual bool WasEverAudible() = 0;

  // Returns whether the renderer is in fullscreen mode.
  virtual bool IsFullscreen() = 0;

  // Returns a copy of the current WebPreferences associated with this
  // WebContents. If it does not exist, this will create one and send the newly
  // computed value to all renderers.
  // Note that this will not trigger a recomputation of WebPreferences if it
  // already exists - this will return the last computed/set value of
  // WebPreferences. If we want to guarantee that the value reflects the current
  // state of the WebContents, NotifyPreferencesChanged() should be called
  // before calling this.
  virtual const blink::web_pref::WebPreferences&
  GetOrCreateWebPreferences() = 0;

  // Notify this WebContents that the preferences have changed, so it needs to
  // recompute the current WebPreferences based on the current state of the
  // WebContents, etc. This will send an IPC to all the renderer processes
  // associated with this WebContents.
  // Note that this will do this by creating a new WebPreferences with default
  // values, then recomputing some of the attributes based on current states.
  // This means if there's any value previously set through SetWebPreferences
  // which does not have special recomputation logic in either
  // WebContentsImpl::ComputeWebPreferences or
  // ContentBrowserClient::OverrideWebkitPrefs, it will return back to its
  // default value whenever this function is called.
  virtual void NotifyPreferencesChanged() = 0;

  // Sets the WebPreferences to |prefs|. This will send an IPC to all the
  // renderer processes associated with this WebContents.
  // Note that this is different from NotifyPreferencesChanged, which recomputes
  // the WebPreferences based on the current state of things. Instead, we're
  // setting this to a specific value. This also means that if we trigger a
  // recomputation of WebPreferences after this, the WebPreferences value will
  // be overridden. if there's any value previously set through
  // SetWebPreferences which does not have special recomputation logic in either
  // WebContentsImpl::ComputeWebPreferences or
  // ContentBrowserClient::OverrideWebkitPrefs, it will return back to its
  // default value, which might be different from the value we set it to here.
  // If you want to use this function outside of tests, consider adding
  // recomputation logic in either of those functions.
  // TODO(rakina): Try to make values set through this function stick even after
  // recomputations.
  virtual void SetWebPreferences(
      const blink::web_pref::WebPreferences& prefs) = 0;

  // Passes current web preferences to all renderer in this WebContents after
  // possibly recomputing them as follows: all "fast" preferences (those not
  // requiring slow platform/device polling) are recomputed unconditionally; the
  // remaining "slow" ones are recomputed only if they have not been computed
  // before.
  //
  // This method must be called if any state that affects web preferences has
  // changed so that it can be recomputed and sent to the renderer.
  virtual void OnWebPreferencesChanged() = 0;

  // Requests the renderer to exit fullscreen.
  // |will_cause_resize| indicates whether the fullscreen change causes a
  // view resize. e.g. This will be false when going from tab fullscreen to
  // browser fullscreen.
  virtual void ExitFullscreen(bool will_cause_resize) = 0;

  // The WebContents is trying to take some action that would cause user
  // confusion if taken while in fullscreen. If this WebContents or any outer
  // WebContents is in fullscreen, drop it.
  //
  // Returns a ScopedClosureRunner, and for the lifetime of that closure, this
  // (and other related) WebContentses will not enter fullscreen. If the action
  // should cause a one-time dropping of fullscreen (e.g. a UI element not
  // attached to the WebContents), invoke RunAndReset() on the returned
  // base::ScopedClosureRunner to release the fullscreen block immediately.
  // Otherwise, if the action should cause fullscreen to be prohibited for a
  // span of time (e.g. a UI element attached to the WebContents), keep the
  // closure alive for that duration.
  //
  // If |display_id| is valid, only WebContentses on that specific screen will
  // exit fullscreen; the scoped prohibition will still apply to all displays.
  // This supports sites using cross-screen window placement capabilities to
  // retain fullscreen and open or place a window on another screen.
  [[nodiscard]] virtual base::ScopedClosureRunner ForSecurityDropFullscreen(
      int64_t display_id) = 0;

  // Unblocks requests from renderer for a newly created window. This is
  // used in showCreatedWindow() or sometimes later in cases where
  // delegate->ShouldResumeRequestsForCreatedWindow() indicated the requests
  // should not yet be resumed. Then the client is responsible for calling this
  // as soon as they are ready.
  virtual void ResumeLoadingCreatedWebContents() = 0;

  // Sets whether the WebContents is for overlaying content on a page.
  virtual void SetIsOverlayContent(bool is_overlay_content) = 0;

  virtual int GetCurrentlyPlayingVideoCount() = 0;

  virtual std::optional<gfx::Size> GetFullscreenVideoSize() = 0;

  // Tells the renderer to clear the focused element (if any).
  virtual void ClearFocusedElement() = 0;

  // Returns true if the current focused element is editable.
  virtual bool IsFocusedElementEditable() = 0;

  // Returns true if a context menu is showing on the page.
  virtual bool IsShowingContextMenu() = 0;

  // Tells the WebContents whether the context menu is showing.
  virtual void SetShowingContextMenu(bool showing) = 0;

#if BUILDFLAG(IS_ANDROID)
  CONTENT_EXPORT static WebContents* FromJavaWebContents(
      const base::android::JavaRef<jobject>& jweb_contents_android);
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents() = 0;

  // Returns the value from CreateParams::java_creator_location.
  virtual base::android::ScopedJavaLocalRef<jthrowable>
  GetJavaCreatorLocation() = 0;

  // Selects and zooms to the find result nearest to the point (x,y) defined in
  // find-in-page coordinates.
  virtual void ActivateNearestFindResult(float x, float y) = 0;

  // Requests the rects of the current find matches from the renderer
  // process. |current_version| is the version of find rects that the caller
  // already knows about. This version will be compared to the current find
  // rects version in the renderer process (which is updated whenever the rects
  // change), to see which new rect data will need to be sent back.
  //
  // TODO(paulmeyer): This process will change slightly once multi-process
  // find-in-page is implemented. This comment should be updated at that time.
  virtual void RequestFindMatchRects(int current_version) = 0;

  // Returns an InterfaceProvider for Java-implemented interfaces that are
  // scoped to this WebContents. This provides access to interfaces implemented
  // in Java in the browser process to C++ code in the browser process.
  virtual service_manager::InterfaceProvider* GetJavaInterfaces() = 0;
#endif  // BUILDFLAG(IS_ANDROID)

  // Returns true if the WebContents has completed its first meaningful paint
  // since the last navigation.
  virtual bool CompletedFirstVisuallyNonEmptyPaint() = 0;

  // TODO(crbug.com/41379215): This is a simple mitigation to validate
  // that an action that requires a user gesture actually has one in the
  // trustworthy browser process, rather than relying on the untrustworthy
  // renderer. This should be eventually merged into and accounted for in the
  // user activation work: crbug.com/848778
  virtual bool HasRecentInteraction() = 0;

  // Causes the WebContents to ignore input events for at least as long as the
  // token exists. In the event of multiple calls, input events will be ignored
  // until all tokens have been destroyed.
  // If WebInputEventAuditCallback is given, it can audits WebInputEvent based
  // input events and ignore only events that the callback returns false for the
  // event. Other kind of events, such as focus event or ui::Events will be
  // always ignored without asking the callback. The given callback will be
  // invoked only while the returned ScopedIgnoreInputEvents alives.
  using WebInputEventAuditCallback =
      base::RepeatingCallback<bool(const blink::WebInputEvent&)>;
  [[nodiscard]] virtual ScopedIgnoreInputEvents IgnoreInputEvents(
      std::optional<WebInputEventAuditCallback> audit_callback) = 0;

  // Returns the group id for all audio streams that correspond to a single
  // WebContents. This can be used to determine if a AudioOutputStream was
  // created from a renderer that originated from this WebContents.
  virtual base::UnguessableToken GetAudioGroupId() = 0;

  // Returns the raw list of favicon candidates as reported to observers via
  // WebContentsObserver::DidUpdateFaviconURL() since the last navigation start.
  // Consider using FaviconDriver in components/favicon if possible for more
  // reliable favicon-related state.
  virtual const std::vector<blink::mojom::FaviconURLPtr>& GetFaviconURLs() = 0;

  // Intended for desktop PWAs with manifest entry of window-controls-overlay,
  // This sends the available title bar area bounds to the renderer process.
  virtual void UpdateWindowControlsOverlay(const gfx::Rect& bounding_rect) = 0;

  // Returns the Window Control Overlay rectangle. Only applies to an
  // outermost main frame's widget. Other widgets always returns an empty rect.
  virtual gfx::Rect GetWindowsControlsOverlayRect() const = 0;

  // Whether the WebContents has an active player that is effectively
  // fullscreen. That means that the video is either fullscreen or it is the
  // content of a fullscreen page (in other words, a fullscreen video with
  // custom controls).
  virtual bool HasActiveEffectivelyFullscreenVideo() = 0;

  // Serialise this object into a trace.
  virtual void WriteIntoTrace(perfetto::TracedValue context) = 0;

  // Returns the value from CreateParams::creator_location.
  virtual const base::Location& GetCreatorLocation() = 0;

  // Returns the parameters associated with PictureInPicture WebContents
  virtual const std::optional<blink::mojom::PictureInPictureWindowOptions>&
  GetPictureInPictureOptions() const = 0;

  // Hide or show the browser controls for the given WebContents, based on
  // allowed states, desired state and whether the transition should be animated
  // or not.
  virtual void UpdateBrowserControlsState(
      cc::BrowserControlsState constraints,
      cc::BrowserControlsState current,
      bool animate,
      const std::optional<cc::BrowserControlsOffsetTagsInfo>&
          offset_tags_info) = 0;

  // Transmits data to V8CrowdsourcedCompileHintsConsumer in the renderer. The
  // data is a model describing which JavaScript functions on the page should be
  // eagerly parsed & compiled by the JS engine.
  virtual void SetV8CompileHints(base::ReadOnlySharedMemoryRegion data) = 0;

  // Sets the last time a tab switch made this WebContents visible.
  // `start_time` is the timestamp of the input event that triggered the tab
  // switch. `destination_is_loaded` is true when
  // ResourceCoordinatorTabHelper::IsLoaded() is true for the new tab contents.
  // These will be used to record metrics with the latency between the input
  // event and the time when the WebContents is painted.
  virtual void SetTabSwitchStartTime(base::TimeTicks start_time,
                                     bool destination_is_loaded) = 0;

  // Checks if the WebContents host pages in preview mode.
  virtual bool IsInPreviewMode() const = 0;

  // Called before ActivatePreviewPage() to prepare the activation. This will
  // end the preview mode and IsInPreviewMode() will start returning false after
  // the call. This allows embedders to run preparation steps on the activating
  // WebContents (e.g. attach TabHelpers) before activating the page shown by
  // the WebContents through ActivatePreviewPage().
  virtual void WillActivatePreviewPage() = 0;

  // Activates the primary page that is shown in preview mode. This will relax
  // capability restriction in the browser process, and notify the renderer to
  // process the prerendering activation algorithm.
  // This all processes happens asynchronously, and
  // `WebContentsDelegate::DidActivatePreviewedPage` will be called once it's
  // done.
  virtual void ActivatePreviewPage() = 0;

  // Starts browser-initiated prefetch, triggered by embedder.
  // - `prefetch_url` is the url the prefetch will be performed.
  // - If `use_prefetch_proxy` is set to true, private prefetch proxy is used in
  //   this prefetch request.
  // - `referrer` is utilized as a value of Referer HTTP request header in this
  //   prefetch request.
  // - `referring_origin` represents the initiator origin of prefetch request,
  //   and is mainly used to regulate prefetch behaviors from some security
  //   perspectives. Normally it should be nullopt and then the opaque origin is
  //   used internally, but if necessary, custom value from trusted surfaces can
  //   be embedded into it here.
  // - `attempt` is used to record some metrics associated with this prefetch
  //   request.
  // - `holdback_status_override` is used to override holdback status, if
  //   specified.
  virtual void StartPrefetch(
      const GURL& prefetch_url,
      bool use_prefetch_proxy,
      const blink::mojom::Referrer& referrer,
      const std::optional<url::Origin>& referring_origin,
      base::WeakPtr<PreloadingAttempt> attempt,
      std::optional<PreloadingHoldbackStatus> holdback_status_override) = 0;

  // Starts an embedder triggered (browser-initiated) prerendering page and
  // returns the unique_ptr<PrerenderHandle>, which cancels prerendering on its
  // destruction. If the prerendering failed to start (e.g. if prerendering is
  // disabled, failure happened or because this URL is already being
  // prerendered), this function returns a nullptr.
  // PreloadingAttempt helps us to log various metrics associated with
  // particular prerendering attempt. `url_match_predicate` allows embedders to
  // define their own predicates for matching same-origin URLs during
  // prerendering activation; it would be useful if embedders want Prerender2 to
  // ignore some parameter mismatches. Note that if the mismatched prerender URL
  // will be activated due to the predicate returning true, the last committed
  // URL in the prerendered RenderFrameHost will be activated.
  // `prerender_navigation_handle_callback` allows embedders to attach their own
  // NavigationHandleUserData when prerender starts, and the user data can be
  // used for identifying the types of embedder for metrics logging.
  virtual std::unique_ptr<PrerenderHandle> StartPrerendering(
      const GURL& prerendering_url,
      PreloadingTriggerType trigger_type,
      const std::string& embedder_histogram_suffix,
      ui::PageTransition page_transition,
      bool should_warm_up_compositor,
      PreloadingHoldbackStatus holdback_status_override,
      PreloadingAttempt* preloading_attempt,
      base::RepeatingCallback<bool(const GURL&,
                                   const std::optional<UrlMatchType>&)>
          url_match_predicate,
      base::RepeatingCallback<void(NavigationHandle&)>
          prerender_navigation_handle_callback) = 0;

  // Cancels all prerendering hosted on this WebContents.
  virtual void CancelAllPrerendering() = 0;

  // May be called when the embedder believes that it is likely that the user
  // will perform a back navigation due to the trigger indicated by `predictor`
  // (e.g. they're hovering over a back button). `disposition` indicates where
  // the navigation is predicted to happen (which could differ from where the
  // navigation actually happens).
  virtual void BackNavigationLikely(PreloadingPredictor predictor,
                                    WindowOpenDisposition disposition) = 0;

  // Returns a scope object that needs to be owned by caller in order to
  // disallow custom cursors. Custom cursors whose width or height are larger
  // than `max_dimension_dips` are diallowed in this web contents for as long as
  // any of the returned `ScopedClosureRunner` objects is alive.
  [[nodiscard]] virtual base::ScopedClosureRunner
  CreateDisallowCustomCursorScope(int max_dimension_dips) = 0;

  // Enables overscroll history navigation.
  virtual void SetOverscrollNavigationEnabled(bool enabled) = 0;

  // Tag `WebContents` with its owner. Used purely for debugging purposes so it
  // does not need to be exhaustive or perfectly correct.
  // TODO(crbug.com/40062641): Remove after bug is fixed.
  virtual void SetOwnerLocationForDebug(
      std::optional<base::Location> owner_location) = 0;

  // Sends the attribution support state to all renderer processes for the
  // current page.
  virtual void UpdateAttributionSupportRenderer() = 0;

  // Return all currently streaming devices of `type` via `callback`.
  virtual void GetMediaCaptureRawDeviceIdsOpened(
      blink::mojom::MediaStreamType type,
      base::OnceCallback<void(std::vector<std::string>)> callback) = 0;

  // Returns an animation manager that displays a preview of the history page
  // during a session history navigation gesture. Only non-null if
  // `features::kBackForwardTransitions` is enabled for the supported platform.
  virtual BackForwardTransitionAnimationManager*
  GetBackForwardTransitionAnimationManager() = 0;

  // Returns the network handle targeting to a specific network. The value
  // `kInvalidNetworkHandle` indicates that the current default network will
  // be bound.
  virtual net::handles::NetworkHandle GetTargetNetwork() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class WebContentsImpl;
  WebContents() = default;
};

}  // namespace content

#if BUILDFLAG(IS_ANDROID)
namespace jni_zero {

// @JniType conversion function.
template <>
inline content::WebContents* FromJniType<content::WebContents*>(
    JNIEnv* env,
    const JavaRef<jobject>& j_obj) {
  return content::WebContents::FromJavaWebContents(j_obj);
}
template <>
inline ScopedJavaLocalRef<jobject> ToJniType(JNIEnv* env,
                                             content::WebContents* obj) {
  return obj->GetJavaWebContents();
}

}  // namespace jni_zero
#endif

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
