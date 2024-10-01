// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_

#include <stdint.h>

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_restrictions.h"
#include "components/viz/common/vertical_scroll_direction.h"
#include "content/common/content_export.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom-forward.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/frame/viewport_intersection_state.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-forward.h"
#include "third_party/blink/public/mojom/media/capture_handle_config.mojom-forward.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class GURL;

namespace blink {
namespace mojom {
class ViewportIntersectionState;
enum class ViewportFit;
}  // namespace mojom
}  // namespace blink

namespace gfx {
class Size;
}  // namespace gfx

namespace ui {
struct AXUpdatesAndEvents;
struct AXLocationAndScrollUpdates;
namespace mojom {
enum class VirtualKeyboardMode;
}  // namespace mojom
}  // namespace ui

namespace network::mojom {
class SharedDictionaryAccessDetails;
}  // namespace network::mojom

namespace content {

class MediaSession;
class NavigationEntry;
class NavigationHandle;
class RenderFrameHost;
class RenderProcessHost;
class RenderViewHost;
class RenderWidgetHost;
class Page;
class WebContents;
struct CookieAccessDetails;
struct EntryChangedDetails;
struct FocusedNodeDetails;
struct GlobalRequestID;
struct LoadCommittedDetails;
struct MediaPlayerId;
struct PrunedDetails;
struct Referrer;
struct TrustTokenAccessDetails;

// Note: before adding a new `WebContentsObserver` subclass, consider if simpler
// helpers will suffice:
//
// - Classes that have a 1:1 relationship with one RenderFrameHost can often
//   use `DocumentUserData` instead.
// - Mojo interface implementations that have a 1 RenderFrameHost to many
//   instances relationship can often use `DocumentService` instead.
// - `base::WeakPtr<WebContents>` and `WeakDocumentPtr` can be used instead of
//   manually clearing raw ptrs using observer methods like
//   `WebContentsDestroyed` or `RenderFrameDeleted`. Similarly, don't create a
//   `WebContentsObserver` just to be able to check for a null
//   `WebContentsObserver::web_contents()`. Use a `base::WeakPtr<WebContents>`
//   instead.
//
// These helpers can help avoid memory safety bugs, such as retaining a pointer
// to a deleted RenderFrameHost, or other security issues, such as origin
// confusion when a RenderFrameHost is reused after a cross-document navigation.

// An observer API implemented by classes which are interested in various page
// events from WebContents.  They also get a chance to filter IPC messages.
// The difference between WebContentsDelegate (WCD) and WebContentsObserver
// (WCO) is that there is one WCD per WebContents and many WCOs. Methods which
// have a return value, e.g. are expected to change state, should be on WCD.
//
// Since a WebContents can be a delegate to almost arbitrarily many
// RenderViewHosts, it is important to check in those WebContentsObserver
// methods which take a RenderViewHost that the event came from the
// RenderViewHost the observer cares about.
//
// Usually, observers should only care about the current RenderViewHost as
// returned by GetRenderViewHost().
class CONTENT_EXPORT WebContentsObserver : public base::CheckedObserver {
 public:
  // Device connection types that can be used by a WebContents.
  enum class DeviceConnectionType {
    // WebUSB
    kUSB,
    // Web Bluetooth
    kBluetooth,
    // WebHID
    kHID,
    // Web Serial
    kSerial
  };

  WebContentsObserver(WebContentsObserver&&) = delete;
  WebContentsObserver(const WebContentsObserver&) = delete;
  WebContentsObserver& operator=(WebContentsObserver&&) = delete;
  WebContentsObserver& operator=(const WebContentsObserver&) = delete;

  // Frames and Views ----------------------------------------------------------

  // Called when a RenderFrame for |render_frame_host| is created in the
  // renderer process. Use |RenderFrameDeleted| to listen for when this
  // RenderFrame goes away.
  virtual void RenderFrameCreated(RenderFrameHost* render_frame_host) {}

  // Called when a RenderFrame for |render_frame_host| is deleted or the
  // renderer process in which it runs it has died. Use |RenderFrameCreated| to
  // listen for when RenderFrame objects are created.
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) {}

  // This method is invoked whenever one of the frames of a WebContents
  // swaps its RenderFrameHost with another one; for example because that frame
  // navigated and the new content is in a different process. The
  // RenderFrameHost that has been replaced is in |old_host|, which can be
  // nullptr if the old RenderFrameHost was shut down or a new frame has been
  // created and no old RenderFrameHost exists.
  //
  // This method, in combination with |FrameDeleted|, is appropriate for
  // observers wishing to track the set of RenderFrameHosts that are current in
  // their frame tree. Note that being current in a frame tree is separate from
  // the lifecycle state of the RenderFrameHosts. Consider using
  // |RenderFrameHostStateChanged|.
  virtual void RenderFrameHostChanged(RenderFrameHost* old_host,
                                      RenderFrameHost* new_host) {}

  // This method is invoked whenever primary page of a WebContents
  // (WebContents::GetPrimaryPage()) changes to `page`. This happens in one of
  // the following cases:
  // 1) when the current RenderFrameHost in the primary main frame changes after
  //    a navigation.
  // 2) when the current RenderFrameHost in the primary main frame is
  //    reinitialized after a crash.
  // 3) when a cross-document navigation commits in the current RenderFrameHost
  //    of the primary main frame.

  // The new primary page might either be a brand new one (if the committed
  // navigation created a new document in the primary main frame) or an existing
  // one (back-forward cache restore or prerendering activation).

  // This notification is not dispatched for changes of pages in the non-primary
  // frame trees (prerendering, fenced frames) and when the primary page is
  // destroyed (e.g., when closing a tab).

  // This method is useful for updating the tab-related UI which depends on the
  // primary page's state (e.g. theme colour, such state should typically be
  // available as a method on a Page or stored in PageUserData). Prefer
  // listening to this method to listening to DidFinishNavigation and checking
  // NavigationHandle::IsInPrimaryMainFrame && !NavigationHandle::IsSameDocument
  // && NavigationHandle::HasCommitted (unless your code has to listen to
  // DidFinishNavigation for some other reason, in which case listening only to
  // DidFinishNavigation is recommended).
  virtual void PrimaryPageChanged(Page& page) {}

  // This method is invoked when a frame is destroyed. A subframe is destroyed
  // when its parent detaches it or navigates to a different document. A main
  // frame is destroyed when the whole WebContents is going away, or, with
  // MPArch, when a non-primary frame tree (e.g., prerendered page) is
  // destroyed.
  // Use |RenderFrameHostChanged| to listen for when a RenderFrameHost object is
  // made the current host for a frame.
  virtual void FrameDeleted(FrameTreeNodeId frame_tree_node_id) {}

  // This method is invoked whenever one of the frames of a WebContents changes
  // its |RenderFrameHost::GetLifecycleState()| i.e., when RenderFrameHost
  // LifecycleState changes from |old_state| to |new_state|. The old
  // LifecycleState that is being changed is |old_state| and the LifecycleState
  // that it changes to is |new_state|. |old_state| and |new_state| are always
  // different, i.e., a frame can't transition to the same state.
  //
  // This method allows code to react to RenderFrameHost LifecycleState changes.
  // For example, to detect changes in the currently active document and perform
  // different actions accordingly, like closing UI/resetting states,
  // RenderFrameHostStateChanged is preferred instead of DidFinishNavigation.
  // DidFinishNavigation does not guarantee that the old document will go away.
  // Instead, it could enter the BackForwardCache and then become kActive later.
  // Features that need to handle transitions like these should monitor
  // LifecycleState changes instead.
  //
  // A particular document may change its LifecycleState and trigger
  // RenderFrameHostStateChanged as part of being committed in a navigation,
  // when another document replaces it by navigating in the same frame, in
  // response to being detached from the DOM, or (with MPArch) as part of
  // activating a prerendered page.
  //
  // When committing a cross-document, cross-RenderFrameHost navigation,
  // navigation-related callbacks are dispatched in the following order:
  // - RenderFrameHostStateChanged(new_frame, old_state, new_state)
  // - RenderFrameHostChanged(old_frame, new_frame)
  // - RenderFrameHostStateChanged(old_frame, old_state, new_state)
  // - DidFinishNavigation(navigation_handle)
  virtual void RenderFrameHostStateChanged(
      RenderFrameHost* render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) {}

  // This method is invoked when something "replaces" the contents of the
  // WebContents or otherwise changes the FrameSinkId of the contents that
  // should be captured if capturing this WebContents, WITHOUT changing the
  // RenderFrameHost.
  virtual void CaptureTargetChanged() {}

  // This method is invoked when the capture handle config changes. This can
  // happen when:
  // 1. The web-application calls |mediaDevices.setCaptureHandleConfig|.
  // 2. The top-level frame is navigated cross-document.
  virtual void OnCaptureHandleConfigUpdate(
      const blink::mojom::CaptureHandleConfig& config) {}

  // This method is invoked when a write-access Captured Surface Control API is
  // successfully invoked by a tab-capturing Web application. These include:
  // * CaptureController.sendWheel()
  // * CaptureController.setZoomLevel()
  //
  // Observing this occurrence allows us to update the UX accordingly; for
  // example, show the user an indicator that the capturing tab is being
  // controlled by the capturing tab.
  virtual void OnCapturedSurfaceControl() {}

  // This method is invoked when the `blink::WebView` of the current
  // RenderViewHost is ready, e.g. because we recreated it after a crash.
  virtual void RenderViewReady() {}

  // This method is invoked when a RenderViewHost of the WebContents is
  // deleted. Note that this does not always happen when the WebContents starts
  // to use a different RenderViewHost, as the old RenderViewHost might get
  // just swapped out.
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}

  // This method is invoked when the process for the current RenderFrameHost
  // of the primary main frame exits (usually by crashing, though possibly by
  // other means). The WebContents continues to use the RenderFrameHost, e.g.
  // when the user reloads the current page. When the RenderFrameHost itself is
  // deleted, the RenderFrameDeleted method will be invoked.
  //
  // Note that this is triggered upstream through
  // RenderProcessHostObserver::RenderProcessExited(); for code that doesn't
  // otherwise need to be a WebContentsObserver, that API is probably a better
  // choice.
  virtual void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) {}

  // This method is invoked when a WebContents swaps its visible RenderViewHost
  // with another one, possibly changing processes. The RenderViewHost that has
  // been replaced is in |old_host|, which is nullptr if the old RVH was shut
  // down.
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) {}

  // This method is invoked when a process in the WebContents becomes
  // unresponsive.
  virtual void OnRendererUnresponsive(RenderProcessHost* render_process_host) {}

  // See WebContentsDelegate::RendererResponsive().
  virtual void OnRendererResponsive(RenderProcessHost* render_process_host) {}

  // Navigation ----------------------------------------------------------------

  // Called when a new navigation starts in the WebContents, WITHOUT
  // guaranteeing that the navigation will either commit or lead to a new
  // document. Consider whether listening to PrimaryPageChanged or
  // DidFinishNavigation is a better fit, and see the IMPORTANT NOTES below.
  //
  // `navigation_handle` is unique to a specific navigation. The same
  // `navigation_handle` will be provided on subsequent calls to
  // DidRedirectNavigation, DidFinishNavigation, and ReadyToCommitNavigation
  // when related to this navigation. Observers should clear any references to
  // `navigation_handle` in DidFinishNavigation, just before it is destroyed.
  //
  // IMPORTANT NOTES:
  // - Starting a navigation doesn't affect which document is shown, or
  // (in many cases) which URL is displayed in the omnibox. Most effects of the
  // navigation only occur at DidFinishNavigation, if it commits. Feature code
  // generally should not use DidStartNavigation to reset their state (e.g.
  // close the UI), especially given that a renderer process can easily start a
  // navigation which is guaranteed not to commit (e.g. by navigating to
  // a URL returning a response with HTTP status code of 204 or a download).
  // Consider listening to PrimaryPageChanged or DidFinishNavigation instead.
  //
  // - This notification is fired by navigations in any frame of the
  // WebContents, not just the primary main frame.
  //
  // - This notification is fired by same-document navigations, such as fragment
  // navigations or pushState/replaceState, which will not result in a document
  // change. To filter these out, use NavigationHandle::IsSameDocument.
  //
  // - There can be more than one navigation ongoing in the same frame at the
  // same time (including the main frame). Each will get its own
  // NavigationHandle.
  //
  // - There is no guarantee that DidFinishNavigation will be called
  // for any particular navigation before DidStartNavigation is called on the
  // next.
  //
  // TODO(creis, mcnee): Consider renaming this method to better indicate its
  // semantics (e.g. DidStartNavigationAttempt).
  //
  // WARNING: Please read the above IMPORTANT NOTES for correct usage.
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
  //
  // Note that using DidFinishNavigation to detect changes in the currently
  // active document / page and reset per-document state is strongly
  // discouraged.
  //
  // Listening to PrimaryPageChanged should be preferred to listening to
  // DidFinishNavigation and checking IsInPrimaryMainFrame, !IsSameDocument, and
  // HasCommitted.
  //
  // The per-document / per-page data should be stored in
  // DocumentUserData / PageUserData instead of resetting it in
  // DidFinishNavigation. (In particular, the page might be stored in the
  // back-forward cache instead of being deleted. See comments in PageUserData /
  // DocumentUserData for more details).
  virtual void DidFinishNavigation(NavigationHandle* navigation_handle) {}

  // Called when the NavigationHandleTiming associated with `navigation_handle`
  // has been updated, which can be triggered by any of these events:
  // - The URLLoader for the network request had started (if the navigation
  //   needs a URLLoader)
  // - A network response has been received (including redirect responses)
  // - The navigation got blocked, and will commit an error page.
  // - The Commit IPC has been sent
  virtual void DidUpdateNavigationHandleTiming(
      NavigationHandle* navigation_handle) {}

  // Called after the WebContents completes the previewed page activation steps.
  // `activation_time` is the time the activation happened.
  virtual void DidActivatePreviewedPage(base::TimeTicks activation_time) {}

  // Document load events ------------------------------------------------------

  // These three methods correspond to the points in time when any document in
  // the frame tree starts loading for the first time (initiates outgoing
  // requests), when incoming data subsequently starts arriving, and when the
  // whole frame tree finishes loading.
  // Notes:
  // - There is no guarantee that calls to DidStartLoading/DidStopLoading are
  //   interleaved (e.g. there can be 2 calls to DidStartLoading in a row).
  // - These functions are different and unrelated from DidFinishLoad, which
  //   is a notification about a specific document instead of the whole frame
  //   tree, and uses a slightly different trigger to signify that the load had
  //   finished.
  virtual void DidStartLoading() {}
  virtual void DidStopLoading() {}

  // The page has made some progress loading. |progress| is a value between 0.0
  // (nothing loaded) to 1.0 (page fully loaded).
  virtual void LoadProgressChanged(double progress) {}

  // This method is invoked once the window.document element of the primary main
  // frame's current document (i.e., |render_frame_host|) is ready. This happens
  // when the document's main HTML resource has finished parsing. Here
  // document element refers to DOMDocument, which is different from browser
  // implementation of blink::Document in DocumentUserData/DocumentService which
  // are typically created when navigation commits.
  //
  // Note that PrimaryMainDocumentElementAvailable should be used when the
  // observers which send IPCs to the renderer want to ensure that
  // window.document is non-null. For for the comment cases like observing
  // primary document/URL changes in the omnibox due to navigation
  // WebContentsObserver::PrimaryPageChanged should be used and to observe fully
  // loaded signal WebContentsObserver::DidFinishLoad can be used.
  //
  // This event is dispatched once in the document's lifetime, which means it's
  // not dispatched after navigation that restores a Back/Forward Cache page.
  // For prerendering, this signal is dispatched when the main document element
  // is available and the document is shown to the user (i.e., after the
  // activation).
  virtual void PrimaryMainDocumentElementAvailable() {}

  // This method is invoked once the onload handler of the primary main frame's
  // current document (i.e., |render_frame_host|) has completed. This happens
  // when the primary main document has finished running onload events after
  // loading all content (images, scripts, etc). Prefer using
  // WebContents::IsDocumentOnLoadCompletedInPrimaryMainFrame instead of saving
  // this state in your component.
  //
  // For prerendering we dispatch DocumentOnLoadCompletedInPrimaryMainFrame on
  // activation whereas for BackForwardCache restores we don't dispatch
  // DocumentOnLoadCompletedInPrimaryMainFrame.
  //
  // DocumentOnLoadCompletedInPrimaryMainFrame is typically used by the
  // embedders to perform actions on a loaded page, for example showing load
  // completion bubbles, injecting scripts which take page snapshots. Note,
  // however, that some web pages might still be loading (i.e. if they
  // dynamically inject content).
  virtual void DocumentOnLoadCompletedInPrimaryMainFrame() {}

  // This method is invoked when we have received a response from the
  // renderer in response to a dom automation controller action.
  // For example, `window.domAutomationController.send(foo())` sends the
  // result of foo() here.
  // |json_string| is a string with the response which came from a specific
  // |render_frame_host|.
  virtual void DomOperationResponse(RenderFrameHost* render_frame_host,
                                    const std::string& json_string) {}

  // This method is invoked when the document in the given frame finished
  // loading. At this point, scripts marked as defer were executed, and
  // content scripts marked "document_end" get injected into the frame.
  virtual void DOMContentLoaded(RenderFrameHost* render_frame_host) {}

  // This method is invoked when the load is done for the document represented
  // by `render_frame_host` on the renderer side, e.g. the onload event was
  // dispatched. Note that this function is not related to
  // `DidStartLoading()` or `DidStopLoading()` as those functions track the
  // loading state of the whole frame tree and uses a different set of triggers
  // to mark the load ending.
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
                           int error_code) {}

  // This method is invoked when the visible security state of the page changes.
  virtual void DidChangeVisibleSecurityState() {}

  // This method is invoked when content was loaded from an in-memory cache.
  virtual void DidLoadResourceFromMemoryCache(
      RenderFrameHost* render_frame_host,
      const GURL& url,
      const std::string& mime_type,
      network::mojom::RequestDestination request_destination) {}

  // This method is invoked when a resource associate with the frame
  // |render_frame_host| has been loaded, successfully or not. |request_id| will
  // only be populated for main frame resources.
  virtual void ResourceLoadComplete(
      RenderFrameHost* render_frame_host,
      const GlobalRequestID& request_id,
      const blink::mojom::ResourceLoadInfo& resource_load_info) {}

  // Called when document reads or sets a cookie (either via document.cookie or
  // issuing a network request).
  // Cookie reads/writes for a dedicated worker are attributed to the
  // RenderFrameHost which created it.
  virtual void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                                 const CookieAccessDetails& details) {}

  // Called when a network request issued by the navigation reads or sets a
  // cookie. If a notification is received after the navigation has committed,
  // it will be attributed to the RenderFrameHost created by the navigation.
  virtual void OnCookiesAccessed(NavigationHandle* navigation_handle,
                                 const CookieAccessDetails& details) {}

  // Called when document accesses a Trust Token (via document.hasTrustToken or
  // issuing a network request).
  // Trust Token accesses for a dedicated worker are attributed to the
  // RenderFrameHost which created it.
  virtual void OnTrustTokensAccessed(RenderFrameHost* render_frame_host,
                                     const TrustTokenAccessDetails& details) {}

  // Called when a network request issued by the navigation accesses a Trust
  // Token. If a notification is received after the navigation has committed, it
  // will be attributed to the RenderFrameHost created by the navigation.
  virtual void OnTrustTokensAccessed(NavigationHandle* navigation_handle,
                                     const TrustTokenAccessDetails& details) {}

  virtual void OnSharedDictionaryAccessed(
      RenderFrameHost* render_frame_host,
      const network::mojom::SharedDictionaryAccessDetails& details) {}
  virtual void OnSharedDictionaryAccessed(
      NavigationHandle* navigation_handle,
      const network::mojom::SharedDictionaryAccessDetails& details) {}

  // Called when the renderer requests access to storage.
  // Observers will be notified about the type of storage access requested
  // as well as whether access was blocked or not.
  virtual void NotifyStorageAccessed(
      RenderFrameHost* render_frame_host,
      blink::mojom::StorageTypeAccessed storage_type,
      bool blocked) {}

  // This method is invoked when a new non-pending navigation entry is created.
  // This corresponds to one NavigationController entry being created
  // (in the case of new navigations) or renavigated to (for back/forward
  // navigations).
  //
  // Note that this function is only dispatched for NavigationEntries of primary
  // pages (e.g. not for prerendered pages or fenced frames).
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
  // The type of the event specifies the kind of interaction. Direct user input
  // signalled through this callback includes:
  // 1) any mouse down event (blink::WebInputEvent::MouseDown);
  // 2) the start of a scroll (blink::WebInputEvent::GestureScrollBegin);
  // 3) any raw key down event (blink::WebInputEvent::RawKeyDown); and
  // 4) any touch event (inc. scrolls) (blink::WebInputEvent::TouchStart).
  virtual void DidGetUserInteraction(const blink::WebInputEvent& event) {}

  // This method is invoked when a RenderViewHost of this WebContents was
  // configured to ignore UI events, and an UI event took place.
  virtual void DidGetIgnoredUIEvent() {}

  // Invoked every time the WebContents changes visibility.
  virtual void OnVisibilityChanged(Visibility visibility) {}

  // Invoked when the primary main frame changes size.
  virtual void PrimaryMainFrameWasResized(bool width_changed) {}

  // Invoked when the given frame changes its window.name property.
  virtual void FrameNameChanged(RenderFrameHost* render_frame_host,
                                const std::string& name) {}

  // Invoked when the color scheme of the primary main document of the
  // WebContents is updated (either because the primary main document's color
  // has been inferred or the primary main document has changed).
  virtual void InferredColorSchemeUpdated(
      std::optional<blink::mojom::PreferredColorScheme> color_scheme) {}

  // Called when a frame receives user activation. This may be called multiple
  // times for the same frame. This should not be used to determine a
  // RenderFrameHost's user activation state. Does not include frames activated
  // by the same-origin visibility heuristic, see `UserActivationState` for
  // details.
  virtual void FrameReceivedUserActivation(RenderFrameHost* render_frame_host) {
  }

  // Called when the given `render_frame_host` successfully completes a Web
  // Authentication API assertion request.
  virtual void WebAuthnAssertionRequestSucceeded(
      RenderFrameHost* render_frame_host) {}

  // Invoked when the display state of the frame changes.
  virtual void FrameDisplayStateChanged(RenderFrameHost* render_frame_host,
                                        bool is_display_none) {}

  // Invoked when a frame changes size.
  virtual void FrameSizeChanged(RenderFrameHost* render_frame_host,
                                const gfx::Size& frame_size) {}

  // This method is invoked when the title of the WebContents is set.
  virtual void TitleWasSet(NavigationEntry* entry) {}

  // These methods are invoked when a Pepper plugin instance is created/deleted
  // in the DOM.
  virtual void PepperInstanceCreated() {}
  virtual void PepperInstanceDeleted() {}

  // This method is called when the viewport fit of a WebContents changes.
  virtual void ViewportFitChanged(blink::mojom::ViewportFit value) {}

  // This method is called when the virtual keyboard mode of a WebContents
  // changes. This can happen as a result of the
  // `navigator.virtualKeyboard.overlaysContent` API or the virtual-keyboard key
  // in the viewport meta tag.
  virtual void VirtualKeyboardModeChanged(ui::mojom::VirtualKeyboardMode mode) {
  }

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

  // Notifies that an |inner_web_contents| instance has been attached to the
  // provided |render_frame_host|. By the time this is called the
  // |inner_web_contents| will have been added to the WebContents tree.
  virtual void InnerWebContentsAttached(WebContents* inner_web_contents,
                                        RenderFrameHost* render_frame_host) {}

  // Invoked when WebContents::Clone() was used to clone a WebContents.
  virtual void DidCloneToNewWebContents(WebContents* old_web_contents,
                                        WebContents* new_web_contents) {}

  // Invoked when the WebContents is being destroyed. Gives subclasses a chance
  // to cleanup. After the whole loop over all WebContentsObservers has been
  // finished, web_contents() returns nullptr.
  virtual void WebContentsDestroyed() {}

  // Called when the user agent override for a WebContents has been changed.
  virtual void UserAgentOverrideSet(
      const blink::UserAgentOverride& ua_override) {}

  // Invoked when new blink::mojom::FaviconURLPtr candidates are received from
  // the renderer process. If the instance is created after the page is loaded,
  // it is recommended to call WebContents::GetFaviconURLs() to get the current
  // list as this callback will not be executed unless there is an update.
  // `render_frame_host` is the main RenderFrameHost for the primary page.
  virtual void DidUpdateFaviconURL(
      RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) {}

  // Called when an audio change occurs to this WebContents. If |audible| is
  // true then one or more frames or child contents are emitting audio; if
  // false, then no frames or child contents are emitting audio. See
  // OnFrameAudioStateChanged for per-frame information.
  virtual void OnAudioStateChanged(bool audible) {}

  // Called when the audio state of an individual frame changes.
  virtual void OnFrameAudioStateChanged(RenderFrameHost* rfh, bool audible) {}

  // Called when an individual remote subframe's intersection with the viewport
  // of the page changes. Note that this value is independent from the
  // visibility of the page.
  //
  // Note: This is only called for remote frames. If you only care about if the
  // frame intersects or not with the viewport, use OnFrameVisibilityChanged()
  // below, as it is called for all frames.
  virtual void OnRemoteSubframeViewportIntersectionStateChanged(
      RenderFrameHost* rfh,
      const blink::mojom::ViewportIntersectionState&
          viewport_intersection_state) {}

  // Called when an individual frame's visibility inside the viewport of the
  // page changes. Note that this value is independent from the visibility of
  // the page.
  virtual void OnFrameVisibilityChanged(
      RenderFrameHost* rfh,
      blink::mojom::FrameVisibility visibility) {}

  // Called when an individual frame starts/stops capturing at least one media
  // stream (audio or video). For example, the frame could be capturing audio
  // from the microphone using getUserMedia(), or it could be capturing another
  // window using getDisplayMedia().
  virtual void OnFrameIsCapturingMediaStreamChanged(
      RenderFrameHost* rfh,
      bool is_capturing_media_stream) {}

  // Called when WebContents starts/stops using a device connection type. The
  // arguments indicate the device connection type that starts/stops being used
  // and whether it is in use (true if it starts being used, false if it stops).
  virtual void OnDeviceConnectionTypesChanged(
      DeviceConnectionType connection_type,
      bool used) {}

  // Invoked when the WebContents is muted/unmuted.
  virtual void DidUpdateAudioMutingState(bool muted) {}

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
  //
  // Note: this is used to observe when the window/tab is being closed, or a
  // GuestView is being attached to the current frame, and NOT used to observe
  // the BeforeUnload events triggered by navigations.
  virtual void BeforeUnloadFired(bool proceed) {}

  // Invoked when a user cancels a before unload dialog.
  virtual void BeforeUnloadDialogCancelled() {}

  // Called whenever the AXTreeID for the main frame has changed.
  virtual void AXTreeIDForMainFrameHasChanged() {}

  // Called when accessibility events or location changes are received
  // from a render frame, but only when the accessibility mode has the
  // ui::AXMode::kWebContents flag set.
  virtual void AccessibilityEventReceived(
      const ui::AXUpdatesAndEvents& details) {}
  virtual void AccessibilityLocationChangesReceived(
      const ui::AXTreeID& tree_id,
      ui::AXLocationAndScrollUpdates& details) {}

  // Invoked when theme color is changed.
  virtual void DidChangeThemeColor() {}

  // Invoked when background color is changed.
  virtual void OnBackgroundColorChanged() {}

  // Invoked when the ColorProvider instance associated with this WebContents
  // has changed.
  virtual void OnColorProviderChanged() {}

  // Called when a message is added to the console of the WebContents. This is
  // invoked before forwarding the message to the WebContents' delegate.
  // |source_id| is a URL. |untrusted_stack_trace| is not present for most
  // messages; only when requested in advance and only for exceptions.
  virtual void OnDidAddMessageToConsole(
      RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) {}

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
  virtual void MediaDestroyed(const MediaPlayerId& id) {}

  // Invoked when a MediaSession associated with this WebContents has been
  // created and initialized.
  virtual void MediaSessionCreated(MediaSession* media_session) {}

  // Invoked when the renderer process changes the page scale factor.
  virtual void OnPageScaleFactorChanged(float page_scale_factor) {}

  // Invoked when a paste event occurs.
  virtual void OnPaste() {}

  // Called when `copied_text` was copied to the clipboard from a given
  // `render_frame_host` within this WebContents. Use this observer instead of
  // ui::ClipboardObserver if you care about the source RenderFrameHost where
  // the copy operation took place.
  // Note: If clipboard copy events for other types of data are later needed by
  // other WebContentsObservers, it'd be okay to generalize this method to
  // support content::ClipboardPasteData directly.
  virtual void OnTextCopiedToClipboard(RenderFrameHost* render_frame_host,
                                       const std::u16string& copied_text) {}

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
  // be invoked before DidUpdateFaviconURL(). `target_frame` is the main render
  // frame host for the primary page.
  virtual void DidUpdateWebManifestURL(RenderFrameHost* target_frame,
                                       const GURL& manifest_url) {}

  // Called when "audible" playback starts or stops on a WebAudio AudioContext.
  using AudioContextId = std::pair<RenderFrameHost*, int>;
  virtual void AudioContextPlaybackStarted(
      const AudioContextId& audio_context_id) {}
  virtual void AudioContextPlaybackStopped(
      const AudioContextId& audio_context_id) {}

  // Called when the RenderFrameHost tries to use a ServiceWorker
  // (e.g. via navigation.serviceWorker API).
  virtual void OnServiceWorkerAccessed(RenderFrameHost* render_frame_host,
                                       const GURL& scope,
                                       AllowServiceWorkerResult allowed) {}
  // Called when the NavigationHandle accesses ServiceWorker to see if the
  // network request should be handled by the ServiceWorker instead
  // (e.g. for navigations to URLs which are in scope of a ServiceWorker).
  virtual void OnServiceWorkerAccessed(NavigationHandle* navigation_handle,
                                       const GURL& scope,
                                       AllowServiceWorkerResult allowed) {}

  // Called when this WebContents is about to be discarded, and replaced with
  // the new, empty `new_contents`. This is an opportunity to transfer to the
  // new WebContents any data that should persist across the discard process.
  // Because the point of tab discarding is to free up memory, careful
  // consideration should be given to transferring data over to the new
  // WebContents. Large amounts of data or data that can be recreated easily
  // shouldn't be transferred, unless its existence is necessary for a feature
  // to work. This will be invoked right after `new_contents` is created, but
  // before its `WasDiscarded` is set to true and before it's attached to a tab
  // strip.
  // TODO(crbug.com/347770670): Remove this once new WebContents are no longer
  // created during discard operations.
  virtual void AboutToBeDiscarded(WebContents* new_contents) {}

  // Called when WebContents has finished its discard operation.
  virtual void WasDiscarded() {}

  // Called when WebContents received a request to lock the keyboard.
  virtual void KeyboardLockRequested() {}

  // Called when WebContents received a request to lock the pointer.
  virtual void PointerLockRequested() {}

  // Called when WebContents received a request to vibrate the page.
  virtual void VibrationRequested() {}

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

  raw_ptr<WebContents, DanglingUntriaged> web_contents_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_OBSERVER_H_
