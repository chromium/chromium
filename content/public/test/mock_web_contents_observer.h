// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_MOCK_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/ax_updates_and_events.h"

namespace content {

// A mock WebContentsObserver with which tests can set expectations for how
// observer methods are called in response to actions taken in the test.
//
// For example:
//  GURL url = ...
//  testing::NiceMock<MockWebContentsObserver> observer(web_contents());
//  EXPECT_CALL(observer,
//              DidStartNavigation(
//                  testing::Truly([&](NavigationHandle* navigation_handle) {
//                    return navigation_handle->GetURL() == url;
//                  })));
//  EXPECT_TRUE(NavigateToURL(shell(), url));
//
class MockWebContentsObserver : public WebContentsObserver {
 public:
  explicit MockWebContentsObserver(WebContents* web_contents);
  ~MockWebContentsObserver() override;

  MOCK_METHOD(void,
              RenderFrameCreated,
              (RenderFrameHost* render_frame_host),
              (override));
  MOCK_METHOD(void,
              RenderFrameDeleted,
              (RenderFrameHost* render_frame_host),
              (override));
  MOCK_METHOD(void, PrimaryPageChanged, (Page & page), (override));
  MOCK_METHOD(void,
              RenderFrameHostChanged,
              (RenderFrameHost* old_host, RenderFrameHost* new_host),
              (override));
  MOCK_METHOD(void,
              FrameDeleted,
              (FrameTreeNodeId frame_tree_node_id),
              (override));
  MOCK_METHOD(void,
              RenderFrameHostStateChanged,
              (RenderFrameHost* render_frame_host,
               RenderFrameHost::LifecycleState old_state,
               RenderFrameHost::LifecycleState new_state),
              (override));
  MOCK_METHOD(void, CaptureTargetChanged, (), (override));
  MOCK_METHOD(void, RenderViewReady, (), (override));
  MOCK_METHOD(void,
              RenderViewDeleted,
              (RenderViewHost* render_view_host),
              (override));
  MOCK_METHOD(void,
              PrimaryMainFrameRenderProcessGone,
              (base::TerminationStatus status),
              (override));
  MOCK_METHOD(void,
              RenderViewHostChanged,
              (RenderViewHost* old_host, RenderViewHost* new_host),
              (override));
  MOCK_METHOD(void,
              OnRendererUnresponsive,
              (RenderProcessHost* render_process_host),
              (override));
  MOCK_METHOD(void,
              OnRendererResponsive,
              (RenderProcessHost* render_process_host),
              (override));
  MOCK_METHOD(void,
              DidStartNavigation,
              (NavigationHandle* navigation_handle),
              (override));
  MOCK_METHOD(void,
              DidRedirectNavigation,
              (NavigationHandle* navigation_handle),
              (override));
  MOCK_METHOD(void,
              ReadyToCommitNavigation,
              (NavigationHandle* navigation_handle),
              (override));
  MOCK_METHOD(void,
              DidFinishNavigation,
              (NavigationHandle* navigation_handle),
              (override));
  MOCK_METHOD(void, DidStartLoading, (), (override));
  MOCK_METHOD(void, DidStopLoading, (), (override));
  MOCK_METHOD(void, LoadProgressChanged, (double progress), (override));
  MOCK_METHOD(void, PrimaryMainDocumentElementAvailable, (), (override));
  MOCK_METHOD(void, DocumentOnLoadCompletedInPrimaryMainFrame, (), (override));
  MOCK_METHOD(void,
              DOMContentLoaded,
              (RenderFrameHost* render_frame_host),
              (override));
  MOCK_METHOD(void,
              DidFinishLoad,
              (RenderFrameHost* render_frame_host, const GURL& validated_url),
              (override));
  MOCK_METHOD(void,
              DidFailLoad,
              (RenderFrameHost* render_frame_host,
               const GURL& validated_url,
               int error_code),
              (override));
  MOCK_METHOD(void, DidChangeVisibleSecurityState, (), (override));
  MOCK_METHOD(void,
              DidLoadResourceFromMemoryCache,
              (RenderFrameHost* render_frame_host,
               const GURL& url,
               const std::string& mime_type,
               network::mojom::RequestDestination request_destination),
              (override));
  MOCK_METHOD(void,
              ResourceLoadComplete,
              (RenderFrameHost* render_frame_host,
               const GlobalRequestID& request_id,
               const blink::mojom::ResourceLoadInfo& resource_load_info),
              (override));
  MOCK_METHOD(void,
              OnCookiesAccessed,
              (RenderFrameHost* render_frame_host,
               const CookieAccessDetails& details),
              (override));
  MOCK_METHOD(void,
              OnCookiesAccessed,
              (NavigationHandle* navigation_handle,
               const CookieAccessDetails& details),
              (override));
  MOCK_METHOD(void,
              NavigationEntryCommitted,
              (const LoadCommittedDetails& load_details),
              (override));
  MOCK_METHOD(void,
              NavigationListPruned,
              (const PrunedDetails& pruned_details),
              (override));
  MOCK_METHOD(void, NavigationEntriesDeleted, (), (override));
  MOCK_METHOD(void,
              NavigationEntryChanged,
              (const EntryChangedDetails& change_details),
              (override));
  MOCK_METHOD(void,
              DidOpenRequestedURL,
              (WebContents* new_contents,
               RenderFrameHost* source_render_frame_host,
               const GURL& url,
               const Referrer& referrer,
               WindowOpenDisposition disposition,
               ui::PageTransition transition,
               bool started_from_context_menu,
               bool renderer_initiated),
              (override));
  MOCK_METHOD(void, DidFirstVisuallyNonEmptyPaint, (), (override));
  MOCK_METHOD(void, NavigationStopped, (), (override));
  MOCK_METHOD(void,
              DidGetUserInteraction,
              (const blink::WebInputEvent& event),
              (override));
  MOCK_METHOD(void, DidGetIgnoredUIEvent, (), (override));
  MOCK_METHOD(void, OnVisibilityChanged, (Visibility visibility), (override));
  MOCK_METHOD(void,
              PrimaryMainFrameWasResized,
              (bool width_changed),
              (override));
  MOCK_METHOD(void,
              FrameNameChanged,
              (RenderFrameHost* render_frame_host, const std::string& name),
              (override));
  MOCK_METHOD(void,
              FrameReceivedUserActivation,
              (RenderFrameHost* render_frame_host),
              (override));
  MOCK_METHOD(void,
              FrameDisplayStateChanged,
              (RenderFrameHost* render_frame_host, bool is_display_none),
              (override));
  MOCK_METHOD(void,
              FrameSizeChanged,
              (RenderFrameHost* render_frame_host,
               const gfx::Size& frame_size),
              (override));
  MOCK_METHOD(void, TitleWasSet, (NavigationEntry * entry), (override));
  MOCK_METHOD(void, PepperInstanceCreated, (), (override));
  MOCK_METHOD(void, PepperInstanceDeleted, (), (override));
  MOCK_METHOD(void,
              ViewportFitChanged,
              (blink::mojom::ViewportFit value),
              (override));
  MOCK_METHOD(void,
              PluginCrashed,
              (const base::FilePath& plugin_path, base::ProcessId plugin_pid),
              (override));
  MOCK_METHOD(void,
              PluginHungStatusChanged,
              (int plugin_child_id,
               const base::FilePath& plugin_path,
               bool is_hung),
              (override));
  MOCK_METHOD(void,
              InnerWebContentsCreated,
              (WebContents* inner_web_contents),
              (override));
  MOCK_METHOD(void,
              InnerWebContentsAttached,
              (WebContents* inner_web_contents,
               RenderFrameHost* render_frame_host),
              (override));
  MOCK_METHOD(void,
              DidCloneToNewWebContents,
              (WebContents* old_web_contents, WebContents* new_web_contents),
              (override));
  MOCK_METHOD(void, WebContentsDestroyed, (), (override));
  MOCK_METHOD(void,
              UserAgentOverrideSet,
              (const blink::UserAgentOverride& ua_override),
              (override));
  MOCK_METHOD(void,
              DidUpdateFaviconURL,
              (RenderFrameHost* render_frame_host,
               const std::vector<blink::mojom::FaviconURLPtr>& candidates),
              (override));
  MOCK_METHOD(void, OnAudioStateChanged, (bool audible), (override));
  MOCK_METHOD(void,
              OnFrameAudioStateChanged,
              (RenderFrameHost* rfh, bool audible),
              (override));
  MOCK_METHOD(void,
              OnDeviceConnectionTypesChanged,
              (DeviceConnectionType connection_type, bool used),
              (override));
  MOCK_METHOD(void, DidUpdateAudioMutingState, (bool muted), (override));
  MOCK_METHOD(void,
              DidToggleFullscreenModeForTab,
              (bool entered_fullscreen, bool will_cause_resize),
              (override));
  MOCK_METHOD(void, DidAcquireFullscreen, (RenderFrameHost* rfh), (override));
  MOCK_METHOD(void,
              DidChangeVerticalScrollDirection,
              (viz::VerticalScrollDirection scroll_direction),
              (override));
  MOCK_METHOD(void, BeforeFormRepostWarningShow, (), (override));
  MOCK_METHOD(void, BeforeUnloadFired, (bool proceed), (override));
  MOCK_METHOD(void, BeforeUnloadDialogCancelled, (), (override));
  MOCK_METHOD(void, AXTreeIDForMainFrameHasChanged, (), (override));
  MOCK_METHOD(void,
              AccessibilityEventReceived,
              (const ui::AXUpdatesAndEvents& details),
              (override));
  MOCK_METHOD(void,
              AccessibilityLocationChangesReceived,
              (const ui::AXTreeID& tree_id,
               ui::AXLocationAndScrollUpdates& details),
              (override));
  MOCK_METHOD(void, DidChangeThemeColor, (), (override));
  MOCK_METHOD(void, OnBackgroundColorChanged, (), (override));
  MOCK_METHOD(void,
              OnDidAddMessageToConsole,
              (RenderFrameHost * source_frame,
               blink::mojom::ConsoleMessageLevel log_level,
               const std::u16string& message,
               int32_t line_no,
               const std::u16string& source_id,
               const std::optional<std::u16string>& untrusted_stack_trace),
              (override));
  MOCK_METHOD(void,
              MediaStartedPlaying,
              (const MediaPlayerInfo& video_type, const MediaPlayerId& id),
              (override));
  MOCK_METHOD(void,
              MediaStoppedPlaying,
              (const MediaPlayerInfo& video_type,
               const MediaPlayerId& id,
               WebContentsObserver::MediaStoppedReason reason),
              (override));
  MOCK_METHOD(void,
              MediaResized,
              (const gfx::Size& size, const MediaPlayerId& id),
              (override));
  MOCK_METHOD(void,
              MediaEffectivelyFullscreenChanged,
              (bool is_fullscreen),
              (override));
  MOCK_METHOD(void,
              MediaPictureInPictureChanged,
              (bool is_picture_in_picture),
              (override));
  MOCK_METHOD(void,
              MediaMutedStatusChanged,
              (const MediaPlayerId& id, bool muted),
              (override));
  MOCK_METHOD(void, MediaDestroyed, (const MediaPlayerId& id), (override));
  MOCK_METHOD(void,
              OnPageScaleFactorChanged,
              (float page_scale_factor),
              (override));
  MOCK_METHOD(void, OnPaste, (), (override));
  MOCK_METHOD(void,
              OnWebContentsFocused,
              (RenderWidgetHost* render_widget_host),
              (override));
  MOCK_METHOD(void,
              OnWebContentsLostFocus,
              (RenderWidgetHost* render_widget_host),
              (override));
  MOCK_METHOD(void,
              OnFocusChangedInPage,
              (FocusedNodeDetails* details),
              (override));
  MOCK_METHOD(void,
              DidUpdateWebManifestURL,
              (RenderFrameHost * target_frame, const GURL& manifest_url),
              (override));
  MOCK_METHOD(void,
              AudioContextPlaybackStarted,
              (const AudioContextId& audio_context_id),
              (override));
  MOCK_METHOD(void,
              AudioContextPlaybackStopped,
              (const AudioContextId& audio_context_id),
              (override));
  MOCK_METHOD(void,
              OnServiceWorkerAccessed,
              (RenderFrameHost* render_frame_host,
               const GURL& scope,
               AllowServiceWorkerResult allowed),
              (override));
  MOCK_METHOD(void,
              OnServiceWorkerAccessed,
              (NavigationHandle* navigation_handle,
               const GURL& scope,
               AllowServiceWorkerResult allowed),
              (override));
  MOCK_METHOD(void,
              AboutToBeDiscarded,
              (WebContents * new_contents),
              (override));
  MOCK_METHOD(void, WasDiscarded, (), (override));
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_WEB_CONTENTS_OBSERVER_H_
