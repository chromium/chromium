// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/optional.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "components/viz/common/vertical_scroll_direction.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/visibility.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/resource_load_info.mojom-forward.h"
#include "content/public/common/resource_type.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace blink {
namespace mojom {
enum class ViewportFit;
}  // namespace mojom
}  // namespace blink

namespace gfx {
class Size;
}  // namespace gfx

namespace content {

class NavigationEntry;
class NavigationHandle;
class RenderFrameHost;
class RenderProcessHost;
class RenderViewHost;
class RenderWidgetHost;
class WebContents;
class WebContentsImpl;
struct AXEventNotificationDetails;
struct AXLocationChangeNotificationDetails;
struct EntryChangedDetails;
struct FaviconURL;
struct FocusedNodeDetails;
struct LoadCommittedDetails;
struct MediaPlayerId;
struct MediaPlayerWatchTime;
struct PrunedDetails;
struct Referrer;

// An observer API implemented by classes which are interested in various page
// load events from WebContents.  They also get a chance to filter IPC messages.
//
// Since a WebContents can be a delegate to almost arbitrarily many
// RenderViewHosts, it is important to check in those WebContentsObserver
// methods which take a RenderViewHost that the event came from the
// RenderViewHost the observer cares about.
//
// Usually, observers should only care about the current RenderViewHost as
// returned by GetRenderViewHost().
//
// TODO(creis, jochen): Hide the fact that there are several RenderViewHosts
// from the WebContentsObserver API. http://crbug.com/173325
class CONTENT_EXPORT WebContentsObserver : public IPC::Listener {
 public:
  // Frames and Views ----------------------------------------------------------

  // Called when a RenderFrame for |render_frame_host| is created in the
  // renderer process. Use |RenderFrameDeleted| to listen for when this
  // RenderFrame goes away.
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) {}

  // Called when a RenderFrame for |render_frame_host| is deleted or the
  // renderer process in which it runs it has died. Use |RenderFrameCreated| to
  // listen for when RenderFrame objects are created.
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) {}

  // This method is invoked whenever one of the current frames of a WebContents
  // swaps its RenderFrameHost with another one; for example because that frame
  // navigated and the new content is in a different process. The
  // RenderFrameHost that has been replaced is in |old_host|, which can be
  // nullptr if the old RenderFrameHost was shut down or a new frame has been
  // created and no old RenderFrameHost exists.
  //
  // This method, in combination with |FrameDeleted|, is appropriate for
  // observers wishing to track the set of current RenderFrameHosts -- i.e.,
  // those hosts that would be visited by calling WebContents::ForEachFrame().
  virtual void RenderFrameHostChanged(RenderFrameHost* old_host,
                                      RenderFrameHost* new_host) {}

  // This method is invoked when a subframe associated with a WebContents is
  // deleted or the WebContents is destroyed and the top-level frame is deleted.
  // Use |RenderFrameHostChanged| to listen for when a RenderFrameHost object is
  // made the current host for a frame.
  virtual void FrameDeleted(RenderFrameHost* render_frame_host) {}

  // This is called when a RVH is created for a WebContents, but not if it's an
  // interstitial.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // Called for every RenderFrameHost that's created for an interstitial.
  virtual void RenderFrameForInterstitialPageCreated(
      RenderFrameHost* render_frame_host) {}

  // This method is invoked when the RenderView of the current RenderViewHost
  // is ready, e.g. because we recreated it after a crash.
  virtual void RenderViewReady() {}

  // This method is invoked when a RenderViewHost of the WebContents is
  // deleted. Note that this does not always happen when the WebContents starts
  // to use a different RenderViewHost, as the old RenderViewHost might get
  // just swapped out.
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}

  // This method is invoked when the process for the current main
  // RenderFrameHost exits (usually by crashing, though possibly by other
  // means). The WebContents continues to use the RenderFrameHost, e.g. when the
  // user reloads the current page. When the RenderFrameHost itself is deleted,
  // the RenderFrameDeleted method will be invoked.
  //
  // Note that this is triggered upstream through
  // RenderProcessHostObserver::RenderProcessExited(); for code that doesn't
  // otherwise need to be a WebContentsObserver, that API is probably a better
  // choice.
  virtual void RenderProcessGone(base::TerminationStatus status) {}

  // This method is invoked when a WebContents swaps its visible RenderViewHost
  // with another one, possibly changing processes. The RenderViewHost that has
  // been replaced is in |old_host|, which is nullptr if the old RVH was shut
  // down.
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) {}

  // This method is invoked when a process in the WebContents becomes
  // unresponsive.
  virtual void OnRendererUnresponsive(RenderProcessHost* render_process_host) {}

  // Navigation ----------------------------------------------------------------

  // Called when a navigation started in the WebContents. |navigation_handle|
  // is unique to a specific navigation. The same |navigation_handle| will be
  // provided on subsequent calls to DidRedirectNavigation, DidFinishNavigation,
  // and ReadyToCommitNavigation when related to this navigation. Observers
  // should clear any references to |navigation_handle| in DidFinishNavigation,
  // just before it is destroyed.
  //
  // Note that this is fired by navigations in any frame of the WebContents,
  // not just the main frame.
  //
  // Note that this is fired by same-document navigations, such as fragment
  // navigations or pushState/replaceState, which will not result in a document
  // change. To filter these out, use NavigationHandle::IsSameDocument.
  //
  // Note that more than one navigation can be ongoing in the same frame at the
  // same time (including the main frame). Each will get its own
  // NavigationHandle.
  //
  // Note that there is no guarantee that DidFinishNavigation will be called
  // for any particular navigation before DidStartNavigation is called on the
  // next.
  virtual void DidStartNavigation(NavigationHandle* navigation_handle) {}

  // Called when a navigation encountered a server redirect.
  virtual void DidRedirectNavigation(NavigationHandle* navigation_handle) {}

  // Called when the navigation is ready to be committed in a renderer. This
  // occurs when the response code isn't 204/205 (which tell the browser that
  // the request is successful but there's no content that follows) or a
  // download (either from a response header or based on mime sniffing the
  // response). The browser then is ready to switch rendering the new document.
  // Most observers should use DidFinishNavigation instead, which happens right
  // after the navigation commits. This method is for observers that want to
  // initialize renderer-side state just before the RenderFrame commits the
  // navigation.
  //
  // This is the first point in time where a RenderFrameHost is associated with
  // the navigation.
  virtual void ReadyToCommitNavigation(NavigationHandle* navigation_handle) {}

  // Called when a navigation finished in the WebContents. This happens when a
  // navigation is committed, aborted or replaced by a new one. To know if the
  // navigation has committed, use NavigationHandle::HasCommitted; use
  // NavigationHandle::IsErrorPage to know if the navigation resulted in an
  // error page.
  //
  // If this is called because the navigation committed, then the document load
  // will still be ongoing in the RenderFrameHost returned by
  // |navigation_handle|. Use the document loads events such as DidStopLoading
  // and related methods to listen for continued events from this
  // RenderFrameHost.
  //
  // Note that this is fired by same-document navigations, such as fragment
  // navigations or pushState/replaceState, which will not result in a document
  // change. To filter these out, use NavigationHandle::IsSameDocument.
  //
  // Note that |navigation_handle| will be destroyed at the end of this call,
  // so do not keep a reference to it afterward.
  virtual void DidFinishNavigation(NavigationHandle* navigation_handle) {}

  // Navigation (obsolete and deprecated) --------------------------------------

  // This method is invoked after the browser process starts a navigation to a
  // pending NavigationEntry. It is not called for renderer-initiated
  // navigations unless they are sent to the browser process via OpenURL. It may
  // be called multiple times for a given navigation, such as a typed URL
  // followed by a cross-process client or server redirect.
  //
  // SOON TO BE DEPRECATED. Use DidStartNavigation instead.
  // TODO(clamy): Remove this function.
  virtual void DidStartNavigationToPendingEntry(const GURL& url,
                                                ReloadType reload_type) {}

  // Document load events ------------------------------------------------------

  // These three methods correspond to the points in time when a document starts
  // loading for the first time (initiates outgoing requests), when incoming
  // data subsequently starts arriving, and when it finishes loading.
  virtual void DidStartLoading() {}
  virtual void DidReceiveResponse() {}
  virtual void DidStopLoading() {}

  // The page has made some progress loading. |progress| is a value between 0.0
  // (nothing loaded) to 1.0 (page fully loaded).
  virtual void LoadProgressChanged(double progress) {}

  // This method is invoked once the window.document object of the main frame
  // was created.
  virtual void DocumentAvailableInMainFrame() {}

  // This method is invoked once the onload handler of the main frame has
  // completed.
  // Prefer using WebContents::IsDocumentOnLoadCompletedInMainFrame instead
  // of saving this state in your component.
  virtual void DocumentOnLoadCompletedInMainFrame() {}

  // This method is invoked when the document in the given frame finished
  // loading. At this point, scripts marked as defer were executed, and
  // content scripts marked "document_end" get injected into the frame.
  virtual void DOMContentLoaded(RenderFrameHost* render_frame_host) {}

  // This method is invoked when the load is done, i.e. the spinner of the tab
  // will stop spinning, and the onload event was dispatched.
  //
  // If the WebContents is displaying replacement content, e.g. network error
  // pages, DidFinishLoad is invoked for frames that were not sending
  // navigational events before. It is safe to ignore these events.
  virtual void DidFinishLoad(RenderFrameHost* render_frame_host,
                             const GURL& validated_url) {}

  // This method is like DidFinishLoad, but when the load failed or was
  // cancelled, e.g. window.stop() is invoked.
  virtual void DidFailLoad(RenderFrameHost* render_frame_host,
                           const GURL& validated_url,
                           int error_code,
                           const base::string16& error_description) {}

  // This method is invoked when the visible security state of the page changes.
  virtual void DidChangeVisibleSecurityState() {}

  // This method is invoked when content was loaded from an in-memory cache.
  virtual void DidLoadResourceFromMemoryCache(
      const GURL& url,
      const std::string& mime_type,
      ResourceType resource_type) {}

  // This method is invoked when a resource associate with the frame
  // |render_frame_host| has been loaded, successfully or not. |request_id| will
  // only be populated for main frame resources.
  virtual void ResourceLoadComplete(
      RenderFrameHost* render_frame_host,
      const GlobalRequestID& request_id,
      const mojom::ResourceLoadInfo& resource_load_info) {}

  // This method is invoked when a document or resource reads a cookie. Note
  // that this isn't tied to any particular navigation (e.g., it may be called
  // after a subsequent navigation commits).
  virtual void OnCookiesRead(const GURL& url,
                             const GURL& first_party_url,
                             const net::CookieList& cookie_list,
                             bool blocked_by_policy) {}

  // This method is invoked when an attempt has been made to set |cookie|. Note
  // that this isn't tied to any particular navigation (e.g., it may be called
  // after a subsequent navigation commits).
  virtual void OnCookieChange(const GURL& url,
                              const GURL& first_party_url,
                              const net::CanonicalCookie& cookie,
                              bool blocked_by_policy) {}

  // This method is invoked when a new non-pending navigation entry is created.
  // This corresponds to one NavigationController entry being created
  // (in the case of new navigations) or renavigated to (for back/forward
  // navigations).
  virtual void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) {}

  // Invoked when the NavigationController decreased its back/forward list count
  // by removing entries from either the front or back of its list. This is
  // usually the result of going back and then doing a new navigation, meaning
  // all the "forward" items are deleted.
  //
  // This normally happens as a result of a new navigation. It will be
  // followed by a NavigationEntryCommitted() call for the new page that
  // caused the pruning. It could also be a result of removing an item from
  // the list to delete history or fix up after interstitials.
  virtual void NavigationListPruned(const PrunedDetails& pruned_details) {}

  // Invoked when NavigationEntries have been deleted because of a history
  // deletion. Observers should ensure that they remove all traces of the
  // deleted entries.
  virtual void NavigationEntriesDeleted() {}

  // Invoked when a NavigationEntry has changed.
  //
  // This will NOT be sent on navigation, interested parties should also
  // implement NavigationEntryCommitted() to handle that case. This will be
  // sent when the entry is updated outside of navigation (like when a new
  // title comes).
  virtual void NavigationEntryChanged(
      const EntryChangedDetails& change_details) {}

  // This method is invoked when a new WebContents was created in response to
  // an action in the observed WebContents, e.g. a link with target=_blank was
  // clicked. The |source_render_frame_host| is the frame in which the action
  // took place.
  virtual void DidOpenRequestedURL(WebContents* new_contents,
                                   RenderFrameHost* source_render_frame_host,
                                   const GURL& url,
                                   const Referrer& referrer,
                                   WindowOpenDisposition disposition,
                                   ui::PageTransition transition,
                                   bool started_from_context_menu,
                                   bool renderer_initiated) {}

  // This method is invoked when the renderer process has completed its first
  // paint after a non-empty layout.
  virtual void DidFirstVisuallyNonEmptyPaint() {}

  // When WebContents::Stop() is called, the WebContents stops loading and then
  // invokes this method. If there are ongoing navigations, their respective
  // failure methods will also be invoked.
  virtual void NavigationStopped() {}

  // Called when there has been direct user interaction with the WebContents.
  // The type argument specifies the kind of interaction. Direct user input
  // signalled through this callback includes:
  // 1) any mouse down event (blink::WebInputEvent::MouseDown);
  // 2) the start of a scroll (blink::WebInputEvent::GestureScrollBegin);
  // 3) any raw key down event (blink::WebInputEvent::RawKeyDown); and
  // 4) any touch event (inc. scrolls) (blink::WebInputEvent::TouchStart).
  virtual void DidGetUserInteraction(const blink::WebInputEvent::Type type) {}

  // This method is invoked when a RenderViewHost of this WebContents was
  // configured to ignore UI events, and an UI event took place.
  virtual void DidGetIgnoredUIEvent() {}

  // Invoked every time the WebContents changes visibility.
  virtual void OnVisibilityChanged(Visibility visibility) {}

  // Invoked when the main frame changes size.
  virtual void MainFrameWasResized(bool width_changed) {}

  // Invoked when the given frame changes its window.name property.
  virtual void FrameNameChanged(RenderFrameHost* render_frame_host,
                                const std::string& name) {}

  // Called when the sticky user activation bit has been set on the frame.
  // This will not be called for new RenderFrameHosts whose underlying
  // FrameTreeNode was already activated. This should not be used to determine a
  // RenderFrameHost's user activation state.
  virtual void FrameReceivedFirstUserActivation(
      RenderFrameHost* render_frame_host) {}

  // Invoked when the display state of the frame changes.
  virtual void FrameDisplayStateChanged(RenderFrameHost* render_frame_host,
                                        bool is_display_none) {}

  // Invoked when a frame changes size.
  virtual void FrameSizeChanged(RenderFrameHost* render_frame_host,
                                const gfx::Size& frame_size) {}

  // This method is invoked when the title of the WebContents is set. Note that
  // |entry| may be null if the web page whose title changed has not yet had a
  // NavigationEntry assigned to it.
  virtual void TitleWasSet(NavigationEntry* entry) {}

  virtual void AppCacheAccessed(const GURL& manifest_url,
                                bool blocked_by_policy) {}

  // These methods are invoked when a Pepper plugin instance is created/deleted
  // in the DOM.
  virtual void PepperInstanceCreated() {}
  virtual void PepperInstanceDeleted() {}

  // This method is called when the viewport fit of a WebContents changes.
  virtual void ViewportFitChanged(blink::mojom::ViewportFit value) {}

  // Notification that a plugin has crashed.
  // |plugin_pid| is the process ID identifying the plugin process. Note that
  // this ID is supplied by the renderer process, so should not be trusted.
  // Besides, the corresponding process has probably died at this point. The ID
  // may even have been reused by a new process.
  virtual void PluginCrashed(const base::FilePath& plugin_path,
                             base::ProcessId plugin_pid) {}

  // Notification that the given plugin has hung or become unhung. This
  // notification is only for Pepper plugins.
  //
  // The plugin_child_id is the unique child process ID from the plugin. Note
  // that this ID is supplied by the renderer process, so should be validated
  // before it's used for anything in case there's an exploited renderer
  // process.
  virtual void PluginHungStatusChanged(int plugin_child_id,
                                       const base::FilePath& plugin_path,
                                       bool is_hung) {}

  // Notifies that an inner WebContents instance has been created with the
  // observed WebContents as its container. |inner_web_contents| has not been
  // added to the WebContents tree at this point, but can be observed safely.
  virtual void InnerWebContentsCreated(WebContents* inner_web_contents) {}

  // Invoked when WebContents::Clone() was used to clone a WebContents.
  virtual void DidCloneToNewWebContents(WebContents* old_web_contents,
                                        WebContents* new_web_contents) {}

  // Invoked when the WebContents is being destroyed. Gives subclasses a chance
  // to cleanup. After the whole loop over all WebContentsObservers has been
  // finished, web_contents() returns nullptr.
  virtual void WebContentsDestroyed() {}

  // Called when the user agent override for a WebContents has been changed.
  virtual void UserAgentOverrideSet(const std::string& user_agent) {}

  // Invoked when new FaviconURL candidates are received from the renderer
  // process.
  virtual void DidUpdateFaviconURL(const std::vector<FaviconURL>& candidates) {}

  // Called when an audio change occurs.
  virtual void OnAudioStateChanged(bool audible) {}

  // Invoked when the WebContents is muted/unmuted.
  virtual void DidUpdateAudioMutingState(bool muted) {}

  // Invoked when a pepper plugin creates and shows or destroys a fullscreen
  // RenderWidget.
  virtual void DidShowFullscreenWidget() {}
  virtual void DidDestroyFullscreenWidget() {}

  // Invoked when the renderer process has toggled the tab into/out of
  // fullscreen mode.
  virtual void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                             bool will_cause_resize) {}

  // Signals that |rfh| has the current fullscreen element. This is invoked
  // when:
  //  1) an element in this frame enters fullscreen or in nested fullscreen, or
  //  2) after an element in a descendant frame exits fullscreen and makes
  //     this frame own the current fullscreen element again.
  virtual void DidAcquireFullscreen(RenderFrameHost* rfh) {}

  // Invoked when an interstitial page is attached or detached.
  virtual void DidAttachInterstitialPage() {}
  virtual void DidDetachInterstitialPage() {}

  // Invoked when the vertical scroll direction of the root layer is changed.
  // Note that if a scroll in a given direction occurs, the scroll is completed,
  // and then another scroll in the *same* direction occurs, we will not
  // consider the second scroll event to have caused a change in direction. Also
  // note that this API will *never* be called with |kNull| which only exists to
  // indicate the absence of a vertical scroll direction.
  virtual void DidChangeVerticalScrollDirection(
      viz::VerticalScrollDirection scroll_direction) {}

  // Invoked before a form repost warning is shown.
  virtual void BeforeFormRepostWarningShow() {}

  // Invoked when the beforeunload handler fires. |proceed| is set to true if
  // the beforeunload can safely proceed, otherwise it should be interrupted.
  // The time is from the renderer process.
  virtual void BeforeUnloadFired(bool proceed,
                                 const base::TimeTicks& proceed_time) {}

  // Invoked when a user cancels a before unload dialog.
  virtual void BeforeUnloadDialogCancelled() {}

  // Called when accessibility events or location changes are received
  // from a render frame, but only when the accessibility mode has the
  // ui::AXMode::kWebContents flag set.
  virtual void AccessibilityEventReceived(
      const AXEventNotificationDetails& details) {}
  virtual void AccessibilityLocationChangesReceived(
      const std::vector<AXLocationChangeNotificationDetails>& details) {}

  // Invoked when theme color is changed to |theme_color|.
  virtual void DidChangeThemeColor(base::Optional<SkColor> theme_color) {}

  // Invoked when media is playing or paused.  |id| is unique per player and per
  // RenderFrameHost.  There may be multiple players within a RenderFrameHost
  // and subsequently within a WebContents.  MediaStartedPlaying() will always
  // be followed by MediaStoppedPlaying() after player teardown.  Observers must
  // release all stored copies of |id| when MediaStoppedPlaying() is received.
  // |has_video| and |has_audio| can both be false in cases where the media
  // is playing muted and should be considered as inaudible for all intent and
  // purposes.
  struct MediaPlayerInfo {
    MediaPlayerInfo(bool has_video, bool has_audio)
        : has_video(has_video), has_audio(has_audio) {}
    bool has_video;
    bool has_audio;
  };

  // Invoked when media playback is interrupted or completed.
  virtual void MediaWatchTimeChanged(
      const content::MediaPlayerWatchTime& watch_time) {}

  virtual void MediaStartedPlaying(const MediaPlayerInfo& video_type,
                                   const MediaPlayerId& id) {}
  enum class MediaStoppedReason {
    // The media was stopped for an unspecified reason.
    kUnspecified,

    // The media was stopped because it reached the end of the stream.
    kReachedEndOfStream,
  };
  virtual void MediaStoppedPlaying(
      const MediaPlayerInfo& video_type,
      const MediaPlayerId& id,
      WebContentsObserver::MediaStoppedReason reason) {}
  virtual void MediaResized(const gfx::Size& size, const MediaPlayerId& id) {}
  // Invoked when media enters or exits fullscreen. We must use a heuristic
  // to determine this as it is not trivial for media with custom controls.
  // There is a slight delay between media entering or exiting fullscreen
  // and it being detected.
  virtual void MediaEffectivelyFullscreenChanged(bool is_fullscreen) {}
  virtual void MediaPictureInPictureChanged(bool is_picture_in_picture) {}
  virtual void MediaMutedStatusChanged(const MediaPlayerId& id, bool muted) {}

  // Invoked when the renderer process changes the page scale factor.
  virtual void OnPageScaleFactorChanged(float page_scale_factor) {}

  // Invoked when a paste event occurs.
  virtual void OnPaste() {}

  // Invoked if an IPC message is coming from a specific RenderFrameHost.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 RenderFrameHost* render_frame_host);

  // Notification that the |render_widget_host| for this WebContents has gained
  // focus.
  virtual void OnWebContentsFocused(RenderWidgetHost* render_widget_host) {}

  // Notification that the |render_widget_host| for this WebContents has lost
  // focus.
  virtual void OnWebContentsLostFocus(RenderWidgetHost* render_widget_host) {}

  // Notification that a RenderFrameHost inside this WebContents has updated
  // its focused element. |details| contains information on the element
  // that has received focus. This allows for observing focus changes
  // within WebContents, as opposed to OnWebContentsFocused/LostFocus
  // which allows observation that the RenderWidgetHost for the
  // WebContents has gained/lost focus.
  virtual void OnFocusChangedInPage(FocusedNodeDetails* details) {}

  // Notifies that the manifest URL for the main frame changed to
  // |manifest_url|. This will be invoked when a document with a manifest loads
  // or when the manifest URL changes (possibly to nothing). It is not invoked
  // when a document with no manifest loads. During document load, if the
  // document has both a manifest and a favicon, DidUpdateWebManifestURL() will
  // be invoked before DidUpdateFaviconURL().
  virtual void DidUpdateWebManifestURL(
      const base::Optional<GURL>& manifest_url) {}

  // Called to give the embedder an opportunity to bind an interface request
  // from a frame. If the request can be bound, |interface_pipe| will be taken.
  virtual void OnInterfaceRequestFromFrame(
      RenderFrameHost* render_frame_host,
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) {}

  // Called when "audible" playback starts or stops on a WebAudio AudioContext.
  using AudioContextId = std::pair<RenderFrameHost*, int>;
  virtual void AudioContextPlaybackStarted(
      const AudioContextId& audio_context_id) {}
  virtual void AudioContextPlaybackStopped(
      const AudioContextId& audio_context_id) {}

  // IPC::Listener implementation.
  // DEPRECATED: Use (i.e. override) the other overload instead:
  //     virtual bool OnMessageReceived(const IPC::Message& message,
  //                                    RenderFrameHost* render_frame_host);
  // TODO(https://crbug.com/758026): Delete this overload when possible.
  bool OnMessageReceived(const IPC::Message& message) override;

  WebContents* web_contents() const;

 protected:
  // Use this constructor when the object is tied to a single WebContents for
  // its entire lifetime.
  explicit WebContentsObserver(WebContents* web_contents);

  // Use this constructor when the object wants to observe a WebContents for
  // part of its lifetime.  It can then call Observe() to start and stop
  // observing.
  WebContentsObserver();

  ~WebContentsObserver() override;

  // Start observing a different WebContents; used with the default constructor.
  void Observe(WebContents* web_contents);

 private:
  friend class WebContentsImpl;

  void ResetWebContents();

  WebContentsImpl* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
