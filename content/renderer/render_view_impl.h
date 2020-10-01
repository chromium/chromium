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
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/input/browser_controls_state.h"
#include "content/common/content_export.h"
#include "content/public/common/browser_controls_state.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_widget.h"
#include "content/renderer/render_widget_delegate.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/feature_policy/feature_policy_features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_widget_client.h"
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
//
// When the main frame is part of this RenderViewImpl's frame tree, then this
// object acts as the RenderWidgetDelegate for that frame's RenderWidget. Other
// RenderWidgets would have a null RenderWidgetDelegate.
//
// Note: There are cases where there may be multiple main frames in tab. For
// example, both Portals and GuestViews create their own RenderView that's
// nested within another RenderView's frame tree. In these cases, the
// RenderWidget for the nested view will have a non-null RenderWidgetDelegate,
// despite the fact that it isn't the root of the hierarchy.
class CONTENT_EXPORT RenderViewImpl : public blink::WebViewClient,
                                      public IPC::Listener,
                                      public RenderWidgetDelegate,
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
      RenderWidget::ShowCallback show_callback,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Instances of this object are created by and destroyed by the browser
  // process. This method must be called exactly once by the IPC subsystem when
  // the browser wishes the object to be destroyed.
  void Destroy();

  // Used by web_test_support to hook into the creation of RenderViewImpls.
  static void InstallCreateHook(RenderViewImpl* (*create_render_view_impl)(
      AgentSchedulingGroup&,
      CompositorDependencies*,
      const mojom::CreateViewParams&));

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

  const blink::mojom::RendererPreferences& renderer_preferences() const {
    return renderer_preferences_;
  }

  void set_send_content_state_immediately(bool value) {
    send_content_state_immediately_ = value;
  }

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

  // A popup widget opened by this view needs to be shown.
  void ShowCreatedPopupWidget(RenderWidget* popup_widget,
                              blink::WebNavigationPolicy policy,
                              const gfx::Rect& initial_rect);
  // A RenderWidgetFullscreen widget opened by this view needs to be shown.
  void ShowCreatedFullscreenWidget(RenderWidget* fullscreen_widget,
                                   blink::WebNavigationPolicy policy,
                                   const gfx::Rect& initial_rect);

  // Returns the length of the session history of this RenderView. Note that
  // this only coincides with the actual length of the session history if this
  // RenderView is the currently active RenderView of a WebContents.
  unsigned GetLocalSessionHistoryLengthForTesting() const;

  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate);

  // Registers a watcher to observe changes in the
  // blink::mojom::RendererPreferences.
  void RegisterRendererPreferenceWatcher(
      mojo::PendingRemote<blink::mojom::RendererPreferenceWatcher> watcher);

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
      const blink::FeaturePolicyFeatureState& opener_feature_state,
      const blink::SessionStorageNamespaceId& session_storage_namespace_id)
      override;
  blink::WebPagePopup* CreatePopup(blink::WebLocalFrame* creator) override;
  base::StringPiece GetSessionStorageNamespaceId() override;
  void PrintPage(blink::WebLocalFrame* frame) override;
  void SetValidationMessageDirection(base::string16* main_text,
                                     base::i18n::TextDirection main_text_hint,
                                     base::string16* sub_text,
                                     base::i18n::TextDirection sub_text_hint);
  bool AcceptsLoadDrops() override;
  void FocusNext() override;
  void FocusPrevious() override;
  bool CanUpdateLayout() override;
  void DidUpdateMainFrameLayout() override;
  blink::WebString AcceptLanguages() override;
  int HistoryBackListCount() override;
  int HistoryForwardListCount() override;
  bool CanHandleGestureEvent() override;
  bool AllowPopupsDuringPageUnload() override;
  void OnPageVisibilityChanged(PageVisibilityState visibility) override;
  void OnPageFrozenChanged(bool frozen) override;
  void ZoomLevelChanged() override;
  void OnSetHistoryOffsetAndLength(int history_offset,
                                   int history_length) override;

  // RenderView implementation -------------------------------------------------

  bool Send(IPC::Message* message) override;
  RenderFrameImpl* GetMainRenderFrame() override;
  int GetRoutingID() override;
  float GetZoomLevel() override;
  const blink::web_pref::WebPreferences& GetBlinkPreferences() override;
  void SetBlinkPreferences(
      const blink::web_pref::WebPreferences& preferences) override;
  blink::WebView* GetWebView() override;
  bool GetContentStateImmediately() override;
  const std::string& GetAcceptLanguages() override;

  // Please do not add your stuff randomly to the end here. If there is an
  // appropriate section, add it there. If not, there are some random functions
  // nearer to the top you can add it to.

  base::WeakPtr<RenderViewImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

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
  friend class DevToolsAgentTest;
  friend class RenderViewImplScaleFactorTest;
  friend class RenderViewImplTest;
  friend class RenderViewTest;
  friend class RendererAccessibilityTest;

  // TODO(nasko): Temporarily friend RenderFrameImpl, so we don't duplicate
  // utility functions needed in both classes, while we move frame specific
  // code away from this class.
  friend class RenderFrameImpl;

  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, EmulatingPopupRect);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, RenderFrameMessageAfterDetach);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, BeginNavigationForWebUI);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DidFailProvisionalLoadWithErrorForError);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DidFailProvisionalLoadWithErrorForCancellation);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ImeComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, InsertCharacters);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, JSBlockSentAfterPageLoad);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, LastCommittedUpdateState);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnHandleKeyboardEvent);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnImeTypeChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnNavStateChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SetBlinkPreferences);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           SetEditableSelectionAndComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, StaleNavigationsIgnored);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DontIgnoreBackAfterNavEntryLimit);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           GetCompositionCharacterBoundsTest);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnNavigationHttpPost);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, UpdateDSFAfterSwapIn);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           BeginNavigationHandlesAllTopLevel);
#if defined(OS_MAC)
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, MacTestCmdUp);
#endif
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SetHistoryLengthAndOffset);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, NavigateFrame);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, BasicRenderFrame);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, TextInputTypeWithPepper);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           MessageOrderInDidChangeSelection);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SendCandidateWindowEvents);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, RenderFrameClearedAfterClose);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, PaintAfterSwapOut);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           SetZoomLevelAfterCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplScaleFactorTest,
                           ConverViewportToScreenWithZoomForDSF);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplEnableZoomForDSFTest,
                           GetCompositionCharacterBoundsTest);

  enum ErrorPageType {
    DNS_ERROR,
    HTTP_404,
    CONNECTION_ERROR,
  };

  // Initialize() is separated out from the constructor because it is possible
  // to accidentally call virtual functions. All RenderViewImpl creation is
  // fronted by the Create() method which ensures Initialize() is always called
  // before any other code can interact with instances of this call.
  void Initialize(CompositorDependencies* compositor_deps,
                  mojom::CreateViewParamsPtr params,
                  RenderWidget::ShowCallback show_callback,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // RenderWidgetDelegate implementation ----------------------------------

  void SetActiveForWidget(bool active) override;
  bool SupportsMultipleWindowsForWidget() override;
  bool ShouldAckSyntheticInputImmediately() override;
  bool AutoResizeMode() override;
  void DidCommitCompositorFrameForWidget() override;
  void DidCompletePageScaleAnimationForWidget() override;
  void ResizeWebWidgetForWidget(
      const gfx::Size& widget_size,
      const gfx::Size& visible_viewport_size,
      cc::BrowserControlsParams browser_controls_params) override;

  // Old WebLocalFrameClient implementations
  // ----------------------------------------

  // RenderViewImpl used to be a WebLocalFrameClient, but now RenderFrameImpl is
  // the WebLocalFrameClient. However, many implementations of
  // WebLocalFrameClient methods still live here and are called from
  // RenderFrameImpl. These implementations are to be moved to RenderFrameImpl
  // <http://crbug.com/361761>.

  static WindowOpenDisposition NavigationPolicyToDisposition(
      blink::WebNavigationPolicy policy);

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // content/common/*_messages.h for the message that the function is handling.
  void OnExecuteEditCommand(const std::string& name, const std::string& value);
  void OnAllowScriptToClose(bool script_can_close);
  void OnCancelDownload(int32_t download_id);

  void OnDeterminePageLanguage();
  void OnDisableScrollbarsForSmallWindows(
      const gfx::Size& disable_scrollbars_size_limit);
  void OnMoveOrResizeStarted();
  void OnExitFullscreen();
  void OnSetRendererPrefs(
      const blink::mojom::RendererPreferences& renderer_prefs);
  void OnSuppressDialogsUntilSwapOut();

  // Page message handlers -----------------------------------------------------
  void SetPageFrozen(bool frozen);

  // Adding a new message handler? Please add it in alphabetical order above
  // and put it in the same position in the .cc file.

  // Misc private functions ----------------------------------------------------

#if defined(OS_ANDROID)
  // Make the video capture devices (e.g. webcam) stop/resume delivering video
  // frames to their clients, depending on flag |suspend|. This is called in
  // response to a RenderView PageHidden/Shown().
  void SuspendVideoCaptureDevices(bool suspend);
#endif

#if defined(OS_MAC)
  void UpdateFontRenderingFromRendererPrefs() {}
#else
  void UpdateFontRenderingFromRendererPrefs();
#endif

  // In OOPIF-enabled modes, this tells each RenderFrame with a pending state
  // update to inform the browser process.
  void SendFrameStateUpdates();

  // RenderFrameImpl accessible state ------------------------------------------
  // The following section is the set of methods that RenderFrameImpl needs
  // to access RenderViewImpl state. The set of state variables are page-level
  // specific, so they don't belong in RenderFrameImpl and should remain in
  // this object.
  base::ObserverList<RenderViewObserver>::Unchecked& observers() {
    return observers_;
  }

// Platform specific theme preferences if any are updated here.
#if defined(OS_WIN)
  void UpdateThemePrefs();
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  void UpdateThemePrefs() {}
#endif

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

  blink::mojom::RendererPreferences renderer_preferences_;
  // These are observing changes in |renderer_preferences_|. This is used for
  // keeping WorkerFetchContext in sync.
  mojo::RemoteSet<blink::mojom::RendererPreferenceWatcher>
      renderer_preference_watchers_;

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

  // History list --------------------------------------------------------------

  // The offset of the current item in the history list.
  int history_list_offset_ = -1;

  // The RenderView's current impression of the history length.  This includes
  // any items that have committed in this process, but because of cross-process
  // navigations, the history may have some entries that were committed in other
  // processes.  We won't know about them until the next navigation in this
  // process.
  int history_list_length_ = 0;

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

  // The SessionStorage namespace that we're assigned to has an ID, and that ID
  // is passed to us upon creation.  WebKit asks for this ID upon first use and
  // uses it whenever asking the browser process to allocate new storage areas.
  blink::SessionStorageNamespaceId session_storage_namespace_id_;

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

  base::WeakPtrFactory<RenderViewImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderViewImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
