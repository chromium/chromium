// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/accessibility_tree_formatter.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/mhtml_generation_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/screen_orientation_delegate.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/stop_find_action.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/web_bundler.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom-forward.h"
#include "third_party/blink/public/mojom/input/pointer_lock_result.mojom.h"
#include "third_party/blink/public/mojom/loader/pause_subresource_loading_handle.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace blink {
namespace mojom {
class RendererPreferences;
}
namespace web_pref {
struct WebPreferences;
}
struct Manifest;
struct UserAgentOverride;
}  // namespace blink

namespace base {
class TimeTicks;
}

namespace device {
namespace mojom {
class WakeLockContext;
}
}

namespace net {
struct LoadStateWithParam;
}

namespace service_manager {
class InterfaceProvider;
}

namespace content {

class BrowserContext;
class BrowserPluginGuestDelegate;
class RenderFrameHost;
class RenderViewHost;
class RenderWidgetHostView;
class WebContentsDelegate;
class WebUI;
struct CustomContextMenuContext;
struct DropData;
struct MHTMLGenerationParams;

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
// Each WebContents has exactly one NavigationController; each
// NavigationController belongs to one WebContents. The NavigationController can
// be obtained from GetController(), and is used to load URLs into the
// WebContents, navigate it backwards/forwards, etc. See navigation_controller.h
// for more details.
class WebContents : public PageNavigator,
                    public base::SupportsUserData {
 public:
  struct CONTENT_EXPORT CreateParams {
    explicit CreateParams(BrowserContext* context);
    CreateParams(const CreateParams& other);
    ~CreateParams();
    CreateParams(BrowserContext* context, scoped_refptr<SiteInstance> site);

    BrowserContext* browser_context;

    // Specifying a SiteInstance here is optional.  It can be set to avoid an
    // extra process swap if the first navigation is expected to require a
    // privileged process.
    scoped_refptr<SiteInstance> site_instance;

    // The process id of the frame initiating the open.
    int opener_render_process_id;

    // The routing id of the frame initiating the open.
    int opener_render_frame_id;

    // If the opener is suppressed, then the new WebContents doesn't hold a
    // reference to its opener.
    bool opener_suppressed;

    // Indicates whether this WebContents was created with a window.opener.
    // This is used when determining whether the WebContents is allowed to be
    // closed via window.close(). This may be true even with a null |opener|
    // (e.g., for blocked popups).
    bool created_with_opener;

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
    bool initially_hidden;

    // If non-null then this WebContents will be hosted by a BrowserPlugin.
    BrowserPluginGuestDelegate* guest_delegate;

    // Used to specify the location context which display the new view should
    // belong. This can be nullptr if not needed.
    gfx::NativeView context;

    // Used to specify that the new WebContents creation is driven by the
    // renderer process. In this case, the renderer-side objects, such as
    // RenderFrame, have already been created on the renderer side, and
    // WebContents construction should take this into account.
    bool renderer_initiated_creation;

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
      // has an initialized RenderView.
      //
      // TODO(lukasza): https://crbug.com/848366: Remove
      // kInitializeAndWarmupRendererProcess value - warming up the renderer by
      // initializing the RenderView is redundant with the warm-up that can be
      // achieved by either 1) warming up the spare renderer before creating
      // WebContents and/or 2) speculative RenderFrameHost used internally
      // during a navigation.
      kInitializeAndWarmupRendererProcess,
    } desired_renderer_state;

    // Sandboxing flags set on the new WebContents.
    network::mojom::WebSandboxFlags starting_sandbox_flags;

    // Value used to set the last time the WebContents was made active, this is
    // the value that'll be returned by GetLastActiveTime(). If this is left
    // default initialized then the value is not passed on to the WebContents
    // and GetLastActiveTime() will return the WebContents' creation time.
    base::TimeTicks last_active_time;

    // Normal WebContents initialization is split between construction and the
    // first time it is shown. Some WebContents are never shown though.
    // Setting this to true will invoke the WebContents delayed initialization
    // that doesn't require visibility.
    bool is_never_visible;
  };

  // Creates a new WebContents.
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

  // Returns the WebContents that owns the RenderViewHost, or nullptr if the
  // render view host's delegate isn't a WebContents.
  CONTENT_EXPORT static WebContents* FromRenderViewHost(RenderViewHost* rvh);

  // Returns the WebContents for the RenderFrameHost. It is unsafe to call this
  // function with an invalid (e.g. destructed) `rfh`.
  CONTENT_EXPORT static WebContents* FromRenderFrameHost(RenderFrameHost* rfh);

  // Returns the WebContents associated with the |frame_tree_node_id|. This may
  // return nullptr if the RenderFrameHost is shutting down.
  CONTENT_EXPORT static WebContents* FromFrameTreeNodeId(
      int frame_tree_node_id);

  // A callback that returns a pointer to a WebContents. The callback can
  // always be used, but it may return nullptr: if the info used to
  // instantiate the callback can no longer be used to return a WebContents,
  // nullptr will be returned instead.
  // The callback should only run on the UI thread and it should always be
  // non-null.
  using Getter = base::RepeatingCallback<WebContents*(void)>;
  // Use this variant for instances that will only run the callback a single
  // time.
  using OnceGetter = base::OnceCallback<WebContents*(void)>;

  // Sets delegate for platform specific screen orientation functionality.
  CONTENT_EXPORT static void SetScreenOrientationDelegate(
      ScreenOrientationDelegate* delegate);

  ~WebContents() override {}

  // Intrinsic tab state -------------------------------------------------------

  // Gets/Sets the delegate.
  virtual WebContentsDelegate* GetDelegate() = 0;
  virtual void SetDelegate(WebContentsDelegate* delegate) = 0;

  // Gets the controller for this WebContents.
  virtual NavigationController& GetController() = 0;

  // Returns the user browser context associated with this WebContents (via the
  // NavigationController).
  virtual content::BrowserContext* GetBrowserContext() = 0;

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
  // See also GetVisibleURL above, which may differ from this URL.
  virtual const GURL& GetLastCommittedURL() = 0;

  // Returns the main frame for the currently active view.
  virtual RenderFrameHost* GetMainFrame() = 0;

  // Returns the focused frame for the currently active view.
  virtual RenderFrameHost* GetFocusedFrame() = 0;

  // Returns the current RenderFrameHost for a given FrameTreeNode ID if it is
  // part of this frame tree, not including frames in any inner WebContents.
  // Returns nullptr if |process_id| does not match the current
  // RenderFrameHost's process ID, to avoid security bugs where callers do not
  // realize a cross-process navigation (and thus privilege change) has taken
  // place. See RenderFrameHost::GetFrameTreeNodeId for documentation on
  // frame_tree_node_id.
  virtual RenderFrameHost* FindFrameByFrameTreeNodeId(int frame_tree_node_id,
                                                      int process_id) = 0;

  // NOTE: This is generally unsafe to use. Use FindFrameByFrameTreeNodeId
  // instead.
  // Returns the current RenderFrameHost for a given FrameTreeNode ID if it is
  // part of this frame tree. This may not match the caller's expectation, if a
  // cross-process navigation (and thus privilege change) has taken place.
  // See RenderFrameHost::GetFrameTreeNodeId for documentation on this ID.
  virtual RenderFrameHost* UnsafeFindFrameByFrameTreeNodeId(
      int frame_tree_node_id) = 0;

  // Calls |on_frame| for each frame in the currently active view.
  // Note: The RenderFrameHost parameter is not guaranteed to have a live
  // RenderFrame counterpart in the renderer process. Callbacks should check
  // IsRenderFrameLive(), as sending IPC messages to it in this case will fail
  // silently.
  virtual void ForEachFrame(
      const base::RepeatingCallback<void(RenderFrameHost*)>& on_frame) = 0;

  // Returns a vector of all RenderFrameHosts in the currently active view in
  // breadth-first traversal order.
  virtual std::vector<RenderFrameHost*> GetAllFrames() = 0;

  // Sends the given IPC to all live frames in this WebContents and returns the
  // number of sent messages (i.e. the number of processed frames).
  virtual int SendToAllFrames(IPC::Message* message) = 0;

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

  // Request a one-time snapshot of the accessibility tree without changing
  // the accessibility mode. |ax_mode| is the accessibility mode to use.
  using AXTreeSnapshotCallback =
      base::OnceCallback<void(const ui::AXTreeUpdate&)>;
  virtual void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback,
                                     ui::AXMode ax_mode) = 0;

  // Causes the current page to be closed, including running its onunload event
  // handler.
  virtual void ClosePage() = 0;

  // Returns the currently active fullscreen widget. If there is none, returns
  // nullptr.
  virtual RenderWidgetHostView* GetFullscreenRenderWidgetHostView() = 0;

  // Returns the theme color for the underlying content as set by the
  // theme-color meta tag if any.
  virtual base::Optional<SkColor> GetThemeColor() = 0;

  // Returns the background color for the underlying content as set by CSS if
  // any.
  virtual base::Optional<SkColor> GetBackgroundColor() = 0;

  // Returns the committed WebUI if one exists, otherwise the pending one.
  virtual WebUI* GetWebUI() = 0;
  virtual WebUI* GetCommittedWebUI() = 0;

  // Sets the user-agent that may be used for navigations in this WebContents.
  // The user-agent is *only* used when
  // NavigationEntry::SetIsOverridingUserAgent(true) is used (the value of
  // is-overriding-user-agent may be specified in LoadURLParams). If
  // |override_in_new_tabs| is true, and the first navigation in the tab is
  // renderer initiated, then is-overriding-user-agent is set to true for the
  // NavigationEntry. See SetRendererInitiatedUserAgentOverrideOption() for
  // details on how renderer initiated navigations are configured.
  virtual void SetUserAgentOverride(const blink::UserAgentOverride& ua_override,
                                    bool override_in_new_tabs) = 0;

  // Configures the value of is-overriding-user-agent for renderer initiated
  // navigations. The default is UA_OVERRIDE_INHERIT. This value does not apply
  // to the first renderer initiated navigation if the tab has no navigations.
  // See SetUserAgentOverride() for details on that.
  virtual void SetRendererInitiatedUserAgentOverrideOption(
      NavigationController::UserAgentOverrideOption option) = 0;

  virtual const blink::UserAgentOverride& GetUserAgentOverride() = 0;

  // Set the accessibility mode so that accessibility events are forwarded
  // to each WebContentsObserver.
  virtual void EnableWebContentsOnlyAccessibilityMode() = 0;

  // Returns true only if the WebContentsObserver accessibility mode is
  // enabled.
  virtual bool IsWebContentsOnlyAccessibilityModeForTesting() = 0;

  // Returns true only if complete accessibility mode is on, meaning there's
  // both renderer accessibility, and a native browser accessibility tree.
  virtual bool IsFullAccessibilityModeForTesting() = 0;

  virtual ui::AXMode GetAccessibilityMode() = 0;

  virtual void SetAccessibilityMode(ui::AXMode mode) = 0;

  virtual std::string DumpAccessibilityTree(
      bool internal,
      std::vector<content::AccessibilityTreeFormatter::PropertyFilter>
          property_filters) = 0;

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
      base::Optional<AccessibilityEventCallback> callback) = 0;

  // Tab navigation state ------------------------------------------------------

  // Returns the current navigation properties, which if a navigation is
  // pending may be provisional (e.g., the navigation could result in a
  // download, in which case the URL would revert to what it was previously).
  virtual const base::string16& GetTitle() = 0;

  // Saves the given title to the navigation entry and does associated work. It
  // will update history and the view with the new title, and also synthesize
  // titles for file URLs that have none. Thus |entry| must have a URL set.
  virtual void UpdateTitleForEntry(NavigationEntry* entry,
                                   const base::string16& title) = 0;

  // Returns the SiteInstance associated with the current page.
  virtual SiteInstance* GetSiteInstance() = 0;

  // Returns whether this WebContents is loading a resource.
  virtual bool IsLoading() = 0;

  // Returns the current load progress.
  virtual double GetLoadProgress() = 0;

  // Returns whether this WebContents is loading and and the load is to a
  // different top-level document (rather than being a navigation within the
  // same document) in the main frame. This being true implies that IsLoading()
  // is also true.
  virtual bool IsLoadingToDifferentDocument() = 0;

  // Returns whether the current main document has reached and finished
  // executing its onload() handler. Corresponds to
  // WebContentsObserver::DocumentOnLoadCompletedInMainFrame().
  virtual bool IsDocumentOnLoadCompletedInMainFrame() = 0;

  // Returns whether this WebContents is waiting for a first-response for the
  // main resource of the page.
  virtual bool IsWaitingForResponse() = 0;

  // Returns the current load state and the URL associated with it.
  // The load state is only updated while IsLoading() is true.
  virtual const net::LoadStateWithParam& GetLoadState() = 0;
  virtual const base::string16& GetLoadStateHost() = 0;

  // Returns the upload progress.
  virtual uint64_t GetUploadSize() = 0;
  virtual uint64_t GetUploadPosition() = 0;

  // Returns the character encoding of the page.
  virtual const std::string& GetEncoding() = 0;

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

  // Internal state ------------------------------------------------------------

  // Indicates whether the WebContents is being captured (e.g., for screenshots
  // or mirroring).  Increment calls must be balanced with an equivalent number
  // of decrement calls.  |capture_size| specifies the capturer's video
  // resolution, but can be empty to mean "unspecified."  The first screen
  // capturer that provides a non-empty |capture_size| will override the value
  // returned by GetPreferredSize() until all captures have ended. |stay_hidden|
  // determines whether to treat the underlying page as user-visible or not.
  virtual void IncrementCapturerCount(const gfx::Size& capture_size,
                                      bool stay_hidden) = 0;
  virtual void DecrementCapturerCount(bool stay_hidden) = 0;
  virtual bool IsBeingCaptured() = 0;
  // Returns true if there is any active capturer that called
  // IncrementCaptureCount() with |stay_hidden|==false.
  virtual bool IsBeingVisiblyCaptured() = 0;

  // Indicates/Sets whether all audio output from this WebContents is muted.
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

  // Indicates whether any frame in the WebContents has native file system
  // handles.
  virtual bool HasNativeFileSystemHandles() = 0;

  // Indicates whether a video is in Picture-in-Picture for |this|.
  virtual bool HasPictureInPictureVideo() = 0;

  // Indicates whether this tab should be considered crashed. The setter will
  // also notify the delegate when the flag is changed.
  virtual bool IsCrashed() = 0;
  virtual void SetIsCrashed(base::TerminationStatus status, int error_code) = 0;

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

  // Get/Set the last time that the WebContents was made active (either when it
  // was created or shown with WasShown()).
  virtual base::TimeTicks GetLastActiveTime() = 0;

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
  // which should be in this WebContents' FrameTree. This outer WebContents
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

  // Returns whether this WebContents is a portal. This returns true even when
  // this WebContents is not attached to its portal host's WebContents tree.
  // This value may change over time due to portal activation and adoption.
  virtual bool IsPortal() = 0;

  // If |IsPortal()| is true, returns this WebContents' portal host's
  // WebContents. Otherwise, returns nullptr.
  virtual WebContents* GetPortalHostWebContents() = 0;

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
  virtual void Paste() = 0;
  virtual void PasteAndMatchStyle() = 0;
  virtual void Delete() = 0;
  virtual void SelectAll() = 0;
  virtual void CollapseSelection() = 0;

  // Adjust the selection starting and ending points in the focused frame by
  // the given amounts. A negative amount moves the selection towards the
  // beginning of the document, a positive amount moves the selection towards
  // the end of the document.
  virtual void AdjustSelectionByCharacterOffset(int start_adjust,
                                                int end_adjust,
                                                bool show_selection_menu) = 0;

  // Replaces the currently selected word or a word around the cursor.
  virtual void Replace(const base::string16& word) = 0;

  // Replaces the misspelling in the current selection.
  virtual void ReplaceMisspelling(const base::string16& word) = 0;

  // Let the renderer know that the menu has been closed.
  virtual void NotifyContextMenuClosed(
      const CustomContextMenuContext& context) = 0;

  // Executes custom context menu action that was provided from Blink.
  virtual void ExecuteCustomContextMenuCommand(
      int action, const CustomContextMenuContext& context) = 0;

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

  // Get the bounds of the View, relative to the parent.
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

  // Saves the given frame's URL to the local filesystem.
  virtual void SaveFrame(const GURL& url,
                         const Referrer& referrer) = 0;

  // Saves the given frame's URL to the local filesystem. The headers, if
  // provided, is used to make a request to the URL rather than using cache.
  // Format of |headers| is a new line separated list of key value pairs:
  // "<key1>: <value1>\r\n<key2>: <value2>".
  virtual void SaveFrameWithHeaders(
      const GURL& url,
      const Referrer& referrer,
      const std::string& headers,
      const base::string16& suggested_filename) = 0;

  // Generate an MHTML representation of the current page conforming to the
  // settings provided by |params| and returning final status information via
  // the callback. See MHTMLGenerationParams for details on generation settings.
  // A resulting |file_size| of -1 represents a failure. Any other value
  // represents the size of the successfully generated file.
  //
  // TODO(https://crbug.com/915966): GenerateMHTML will eventually be removed
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

  // Generates a Web Bundle representation of the current page.
  virtual void GenerateWebBundle(
      const base::FilePath& file_path,
      base::OnceCallback<void(uint64_t /* file_size */,
                              data_decoder::mojom::WebBundlerError)>
          callback) = 0;

  // Returns the contents MIME type after a navigation.
  virtual const std::string& GetContentsMimeType() = 0;

  // Returns the settings which get passed to the renderer.
  virtual blink::mojom::RendererPreferences* GetMutableRendererPrefs() = 0;

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

  // Called when the response to a pending mouse lock request has arrived.
  // Returns true if |allowed| is true and the mouse has been successfully
  // locked.
  virtual bool GotResponseToLockMouseRequest(
      blink::mojom::PointerLockResult result) = 0;

  // Wrapper around GotResponseToLockMouseRequest to fit into
  // ChromeWebViewPermissionHelperDelegate's structure.
  virtual void GotLockMousePermissionResponse(bool allowed) = 0;

  // Called when the response to a keyboard mouse lock request has arrived.
  // Returns false if the request is no longer valid, otherwise true.
  virtual bool GotResponseToKeyboardLockRequest(bool allowed) = 0;

  // Called when the user has selected a color in the color chooser.
  virtual void DidChooseColorInColorChooser(SkColor color) = 0;

  // Called when the color chooser has ended.
  virtual void DidEndColorChooser() = 0;

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
  // original opener doesn't reflect window.opener which can be suppressed or
  // updated. This traces all the way back, so if the original owner was closed,
  // but _it_ had an original owner, this will return the original owner's
  // original owner, etc.
  virtual bool HasOriginalOpener() = 0;

  // Returns the original opener if HasOriginalOpener() is true, or nullptr
  // otherwise.
  virtual RenderFrameHost* GetOriginalOpener() = 0;

  // Returns the WakeLockContext accociated with this WebContents.
  virtual device::mojom::WakeLockContext* GetWakeLockContext() = 0;

  // |http_status_code| can be 0 e.g. for data: URLs.
  // |bitmaps| will be empty on download failure.
  // |sizes| are the sizes in pixels of the bitmaps before they were resized due
  // to the max bitmap size passed to DownloadImage(). Each entry in the bitmaps
  // vector corresponds to an entry in the sizes vector. If a bitmap was
  // resized, there should be a single returned bitmap.
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
  // Bitmaps with pixel sizes larger than |max_bitmap_size| are filtered out
  // from the bitmap results. If there are no bitmap results <=
  // |max_bitmap_size|, the smallest bitmap is resized to |max_bitmap_size| and
  // is the only result. A |max_bitmap_size| of 0 means unlimited.
  // For vector images, |preferred_size| will serve as a viewport into which
  // the image will be rendered. This would usually be the dimensions of the
  // square where the bitmap will be rendered. If |preferred_size| is 0, any
  // existing intrinsic dimensions of the image will be used. If
  // |max_bitmap_size| is non-zero it will also impose an upper bound on the
  // preferred size.
  // If |bypass_cache| is true, |url| is requested from the server even if it
  // is present in the browser cache.
  virtual int DownloadImage(const GURL& url,
                            bool is_favicon,
                            uint32_t preferred_size,
                            uint32_t max_bitmap_size,
                            bool bypass_cache,
                            ImageDownloadCallback callback) = 0;

  // Same as DownloadImage(), but uses the ImageDownloader from the specified
  // frame instead of the main frame.
  virtual int DownloadImageInFrame(
      const GlobalFrameRoutingId& initiator_frame_routing_id,
      const GURL& url,
      bool is_favicon,
      uint32_t preferred_size,
      uint32_t max_bitmap_size,
      bool bypass_cache,
      ImageDownloadCallback callback) = 0;

  // Finds text on a page. |search_text| should not be empty.
  virtual void Find(int request_id,
                    const base::string16& search_text,
                    blink::mojom::FindOptionsPtr options) = 0;

  // Notifies the renderer that the user has closed the FindInPage window
  // (and what action to take regarding the selection).
  virtual void StopFinding(StopFindAction action) = 0;

  // Returns true if audio has been audible from the WebContents since the last
  // navigation.
  virtual bool WasEverAudible() = 0;

  // The callback invoked when the renderer responds to a request for the main
  // frame document's manifest. The url will be empty if the document specifies
  // no manifest, and the manifest will be empty if any other failures occurred.
  using GetManifestCallback =
      base::OnceCallback<void(const GURL&, const blink::Manifest&)>;

  // Requests the manifest URL and the Manifest of the main frame's document.
  virtual void GetManifest(GetManifestCallback callback) = 0;

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
  virtual base::ScopedClosureRunner ForSecurityDropFullscreen(
      int64_t display_id = display::kInvalidDisplayId) WARN_UNUSED_RESULT = 0;

  // Unblocks requests from renderer for a newly created window. This is
  // used in showCreatedWindow() or sometimes later in cases where
  // delegate->ShouldResumeRequestsForCreatedWindow() indicated the requests
  // should not yet be resumed. Then the client is responsible for calling this
  // as soon as they are ready.
  virtual void ResumeLoadingCreatedWebContents() = 0;

  // Sets whether the WebContents is for overlaying content on a page.
  virtual void SetIsOverlayContent(bool is_overlay_content) = 0;

  virtual int GetCurrentlyPlayingVideoCount() = 0;

  virtual base::Optional<gfx::Size> GetFullscreenVideoSize() = 0;

  // Tells the renderer to clear the focused element (if any).
  virtual void ClearFocusedElement() = 0;

  // Returns true if the current focused element is editable.
  virtual bool IsFocusedElementEditable() = 0;

  // Returns true if a context menu is showing on the page.
  virtual bool IsShowingContextMenu() = 0;

  // Tells the WebContents whether the context menu is showing.
  virtual void SetShowingContextMenu(bool showing) = 0;

#if defined(OS_ANDROID)
  CONTENT_EXPORT static WebContents* FromJavaWebContents(
      const base::android::JavaRef<jobject>& jweb_contents_android);
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents() = 0;

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
#endif  // OS_ANDROID

  // Returns true if the WebContents has completed its first meaningful paint
  // since the last navigation.
  virtual bool CompletedFirstVisuallyNonEmptyPaint() = 0;

  // TODO(https://crbug.com/826293): This is a simple mitigation to validate
  // that an action that requires a user gesture actually has one in the
  // trustworthy browser process, rather than relying on the untrustworthy
  // renderer. This should be eventually merged into and accounted for in the
  // user activation work.
  virtual bool HasRecentInteractiveInputEvent() = 0;

  // Sets a flag that causes the WebContents to ignore input events.
  virtual void SetIgnoreInputEvents(bool ignore_input_events) = 0;

  // Returns the group id for all audio streams that correspond to a single
  // WebContents. This can be used to determine if a AudioOutputStream was
  // created from a renderer that originated from this WebContents.
  virtual base::UnguessableToken GetAudioGroupId() = 0;

  // Returns the raw list of favicon candidates as reported to observers via
  // WebContentsObserver::DidUpdateFaviconURL() since the last navigation start.
  // Consider using FaviconDriver in components/favicon if possible for more
  // reliable favicon-related state.
  virtual const std::vector<blink::mojom::FaviconURLPtr>& GetFaviconURLs() = 0;

 private:
  // This interface should only be implemented inside content.
  friend class WebContentsImpl;
  WebContents() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
