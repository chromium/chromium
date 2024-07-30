// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/common/performance/performance_timeline_constants.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/public/common/subresource_load_metrics.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_navigation_type.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/page_transition_types.h"
#include "v8/include/v8-forward.h"

class GURL;

namespace blink {
class WebDocumentLoader;
class WebElement;
class WebFormElement;
class WebString;
class WebURLRequest;
class WebWorkerFetchContext;
enum class DetachReason;
struct JavaScriptFrameworkDetectionResult;
}  // namespace blink

namespace gfx {
class Rect;
}  // namespace gfx

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace url {
class SchemeHostPort;
}  // namespace url

namespace content {

class RendererPpapiHost;
class RenderFrame;

// Base class for objects that want to filter incoming IPCs, and also get
// notified of changes to the frame.
class CONTENT_EXPORT RenderFrameObserver
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
    : public IPC::Listener,
      public IPC::Sender
#endif
{
 public:
  RenderFrameObserver(const RenderFrameObserver&) = delete;
  RenderFrameObserver& operator=(const RenderFrameObserver&) = delete;

  // A subclass can use this to delete itself. If it does not, the subclass must
  // always null-check each call to render_frame() because the RenderFrame can
  // go away at any time.
  virtual void OnDestruct() = 0;

  // Called when a Pepper plugin is created.
  virtual void DidCreatePepperPlugin(RendererPpapiHost* host) {}

  // Called when a load is explicitly stopped by the user or browser.
  virtual void OnStop() {}

  // Called when the RenderFrame visiblity is changed.
  virtual void WasHidden() {}
  virtual void WasShown() {}

  // Called when the RenderFrame's visibility status changes.
  virtual void OnFrameVisibilityChanged(
      blink::mojom::FrameVisibility render_status) {}

  // Navigation callbacks.
  //
  // Each navigation starts with a DidStartNavigation call. Then it may be
  // followed by a ReadyToCommitNavigation (if the navigation has succeeded),
  // and should always end with a DidFinishNavigation.
  // TODO(dgozman): ReadyToCommitNavigation will be removed soon.
  //
  // Unfortunately, this is currently a mess. For example, some started
  // navigations which did not commit won't receive any further notifications.
  // DidCommitProvisionalLoad will be called for same-document navigations,
  // without any other notifications. DidFailProvisionalLoad will be called
  // when committing error pages, in addition to all the methods (start, ready,
  // commit) for the error page load itself.

  // Called when the RenderFrame has started a navigation.
  // |url| is a url being navigated to. Note that final url might be different
  // due to redirects.
  // |navigation_type| is only present for renderer-initiated navigations, e.g.
  // JavaScript call, link click, form submit. User-initiated navigations from
  // the browser process (e.g. by typing a url) won't have a navigation type.
  virtual void DidStartNavigation(
      const GURL& url,
      std::optional<blink::WebNavigationType> navigation_type) {}

  // Called when a navigation has just committed and |document_loader|
  // will start loading a new document in the RenderFrame.
  // TODO(dgozman): the name does not match functionality anymore, we should
  // merge this with DidCommitProvisionalLoad, which will become
  // DidFinishNavigation.
  virtual void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) {}

  // Called when a RenderFrame's page lifecycle state gets updated.
  virtual void DidSetPageLifecycleState(bool restoring_from_bfcache) {}

  // These match the Blink API notifications. These will not be called for the
  // initial empty document, since that already exists before an observer for a
  // frame has a chance to be created (before notification about the RenderFrame
  // being created occurs).
  virtual void DidCreateNewDocument() {}
  virtual void DidCreateDocumentElement() {}
  // TODO(dgozman): replace next two methods with DidFinishNavigation.
  // DidCommitProvisionalLoad is only called for new-document navigations.
  // Use DidFinishSameDocumentNavigation for same-document navigations.
  virtual void DidCommitProvisionalLoad(ui::PageTransition transition) {}
  virtual void DidFailProvisionalLoad() {}
  virtual void DidFinishLoad() {}
  virtual void DidFinishLoadForPrinting() {}
  virtual void DidDispatchDOMContentLoadedEvent() {}
  virtual void DidHandleOnloadEvents() {}
  virtual void DidCreateScriptContext(v8::Local<v8::Context> context,
                                      int32_t world_id) {}
  virtual void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                        int32_t world_id) {}
  virtual void DidClearWindowObject() {}
  virtual void DidChangeScrollOffset() {}
  virtual void WillSubmitForm(const blink::WebFormElement& form) {}
  virtual void DidMatchCSS(
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors) {}

  // Called when the RenderFrame creates a FencedFrame and provides the
  // RemoteFrameToken to identify the `blink::RemoteFrame` to the inner
  // RenderFrame. This is called immediately after the FencedFrame is created
  // in the browser and the `blink::RemoteFrame` initialized in this renderer.
  virtual void DidCreateFencedFrame(
      const blink::RemoteFrameToken& placeholder_token) {}

  // Called when same-document navigation finishes.
  // This is the only callback for same-document navigations,
  // DidStartNavigation and ReadyToCommitNavigation are not called.
  //
  // Same-document navigation is typically initiated by an anchor click
  // (that usually results in the page scrolling to the anchor) or a
  // history web API manipulation.
  //
  // However, it could be some rare case like user changing #hash in the url
  // bar or history restore for subframe or anything else that was classified
  // as same-document.
  virtual void DidFinishSameDocumentNavigation() {}

  // Called when this RenderFrame has been detached from the view.  Note that
  // this refers to the detachment of the RenderFrame object, not the "browsing
  // context". This means that WillDetach can fire as a result of navigating
  // within the same browsingcontext that creates a new RenderFrame (either in
  // this process or a different process), on top of being fired when the
  // browsing context is actually detached (including when the parent
  // RenderFrame is being detached). See comments for blink::DetachReason for
  // more details.
  virtual void WillDetach(blink::DetachReason detach_reason) {}

  // Called when we receive a console message from Blink for which we requested
  // extra details (like the stack trace). |message| is the error message,
  // |source| is the Blink-reported source of the error (either external or
  // internal), and |stack_trace| is the stack trace of the error in a
  // human-readable format (each frame is formatted as
  // "\n    at function_name (source:line_number:column_number)").
  virtual void DetailedConsoleMessageAdded(
      const std::u16string& message,
      const std::u16string& source,
      const std::u16string& stack_trace,
      uint32_t line_number,
      blink::mojom::ConsoleMessageLevel level) {}

  // Called when an interesting (from document lifecycle perspective),
  // compositor-driven layout had happened. This is a reasonable hook to use
  // to inspect the document and layout information, since it is in a clean
  // state and you won't accidentally force new layouts.
  // The interestingness of layouts is explained in WebMeaningfulLayout.h.
  virtual void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {}

  // Notifications when |PerformanceTiming| data becomes available
  virtual void DidChangePerformanceTiming() {}

  // Notifications when a user interaction latency data becomes available. A
  // user interaction can be built up from multiple input events (e.g. keydown
  // then keyup). Each of these events has an input to next frame latency. This
  // reports the timings of the max input-to-frame latency for each interaction.
  // `max_event_start` is when input was received, `max_event_end` is when
  // the next frame was presented, `max_event_queued_main_thread` is when the
  // input was queued and `max_event_commit_finish` is when the next commit
  // finished after event has been processed. See
  // https://web.dev/inp/#whats-in-an-interaction for more detailed motivation
  // and explanation.
  virtual void DidObserveUserInteraction(
      base::TimeTicks max_event_start,
      base::TimeTicks max_event_queued_main_thread,
      base::TimeTicks max_event_commit_finish,
      base::TimeTicks max_event_end,
      blink::UserInteractionType interaction_type,
      uint64_t interaction_offset) {}

  // Notification when the First Scroll Delay becomes available.
  virtual void DidObserveFirstScrollDelay(base::TimeDelta first_scroll_delay) {}

  // Notifications when a cpu timing update becomes available, when a frame
  // has performed at least 100ms of tasks.
  virtual void DidChangeCpuTiming(base::TimeDelta time) {}

  // Notification when the renderer uses a particular code path during a page
  // load. This is used for metrics collection.
  virtual void DidObserveLoadingBehavior(blink::LoadingBehaviorFlag behavior) {}

  // Notification when the renderer performed framework detection during a page
  // load. This is used for metrics collection.
  virtual void DidObserveJavaScriptFrameworks(
      const blink::JavaScriptFrameworkDetectionResult&) {}

  // Notification when the renderer uses subresources.
  // It is called when there is a subresouce load. The reported values via
  // arguments are cumulative. They are NOT a difference from the previous call.
  virtual void DidObserveSubresourceLoad(
      const blink::SubresourceLoadMetrics& subresource_load_metrics) {}

  // Notification when the renderer observes a new use counter usage during a
  // page load. This is used for UseCounter metrics.
  virtual void DidObserveNewFeatureUsage(
      const blink::UseCounterFeature& feature) {}

  // A new soft navigation was observed.
  // A soft navigation is:
  // - A same-document navigation in the top-level document.
  // - Triggered with a user gesture.
  // - Initiated with the window.history or window.navigation APIs.
  // - Accompanied with a DOM modification of the <main> element during the same
  // or a descendant task.
  virtual void DidObserveSoftNavigation(blink::SoftNavigationMetrics metrics) {}

  // Reports that visible elements in the frame shifted (bit.ly/lsm-explainer).
  // This is called once for each animation frame containing any layout shift,
  // and receives the layout shift (LS) score for that frame.  The cumulative
  // layout shift (CLS) score can be inferred by summing the LS scores.
  // |after_input_or_scroll| indicates whether the given |score| was observed
  // after an input or scroll occurred in the associated document.
  virtual void DidObserveLayoutShift(double score, bool after_input_or_scroll) {
  }

  // Reports that a resource will be requested.
  virtual void WillSendRequest(const blink::WebURLRequest& request) {}

  // Notification when the renderer a response started, completed or canceled.
  // Complete or Cancel is guaranteed to be called for a response that started.
  // |request_id| uniquely identifies the request within this render frame.
  virtual void DidStartResponse(
      const url::SchemeHostPort& final_response_url,
      int request_id,
      const network::mojom::URLResponseHead& response_head,
      network::mojom::RequestDestination request_destination,
      bool is_ad_resource) {}
  virtual void DidCompleteResponse(
      int request_id,
      const network::URLLoaderCompletionStatus& status) {}
  virtual void DidCancelResponse(int request_id) {}

  // Reports that a resource was loaded from the blink memory cache.
  // |request_id| uniquely identifies this resource within this render frame.
  // |from_archive| indicates if the resource originated from a MHTML archive.
  virtual void DidLoadResourceFromMemoryCache(const GURL& response_url,
                                              int request_id,
                                              int64_t encoded_body_length,
                                              const std::string& mime_type,
                                              bool from_archive) {}

  // Notification when the renderer observes data used during the page load.
  // This is used for page load metrics. |received_data_length| is the received
  // network bytes. |resource_id| uniquely identifies the resource within this
  // render frame.
  virtual void DidReceiveTransferSizeUpdate(int resource_id,
                                            int received_data_length) {}

  // Called when the focused element has changed to |element|.
  virtual void FocusedElementChanged(const blink::WebElement& element) {}

  // Called when accessibility is enabled or disabled.
  virtual void AccessibilityModeChanged(const ui::AXMode& mode) {}

  // Called when script in the page calls window.print().
  virtual void ScriptedPrint(bool user_initiated) {}

  // Called when a worker fetch context will be created.
  virtual void WillCreateWorkerFetchContext(blink::WebWorkerFetchContext*) {}

  // For the main frame, called when the main frame's dimensions have changed,
  // e.g. resizing a tab causes the document width to change; loading additional
  // content causes the document height to increase; explicitly changing the
  // height of the body element.
  //
  // For a subframe, called when the intersection rect between the main frame
  // and the subframe has changed, e.g. the subframe is initially added; the
  // subframe's position is updated explicitly or inherently (e.g. sticky
  // position while the page is being scrolled).
  virtual void OnMainFrameIntersectionChanged(
      const gfx::Rect& main_frame_intersection_rect) {}

  // Called when the main frame's viewport rectangle (the viewport dimensions
  // and the scroll position) changed, e.g. the user scrolled the main frame or
  // the viewport dimensions themselves changed. Only invoked on the main frame.
  virtual void OnMainFrameViewportRectangleChanged(
      const gfx::Rect& main_frame_viewport_rect) {}

  // Called when an image ad rectangle changed. An empty `image_ad_rect` is used
  // to signal the removal of the rectangle. Only invoked on the main frame.
  virtual void OnMainFrameImageAdRectangleChanged(
      int element_id,
      const gfx::Rect& image_ad_rect) {}

  // Overlay-popup-ad violates The Better Ads Standards
  // (https://www.betterads.org/standards/). This method will be called when an
  // overlay-popup-ad is detected, to let the embedder
  // (i.e. subresource_filter::ContentSubresourceFilterThrottleManager) know the
  // violation so as to apply further interventions.
  virtual void OnOverlayPopupAdDetected() {}

  // Large-sticky-ad violates The Better Ads Standards
  // (https://www.betterads.org/standards/). This method will be called when a
  // large-sticky-ad is detected, to let the embedder
  // (i.e. subresource_filter::ContentSubresourceFilterThrottleManager) know the
  // violation so as to apply further interventions.
  virtual void OnLargeStickyAdDetected() {}

  // Called to give the embedder an opportunity to bind an interface request
  // for a frame. If the request can be bound, |interface_pipe| will be taken.
  virtual void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) {}

  // Similar to above but for handling Channel-associated interface requests.
  // Returns |true| if the request is handled by the implementation (taking
  // ownership of |*handle|) and |false| otherwise (leaving |*handle|
  // unmodified).
  virtual bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle);

  // The smoothness metrics is shared over shared-memory. The interested
  // observer should invalidate |shared_memory| (by std::move()'ing it), and
  // return true. All other observers should return false (default).
  virtual bool SetUpSmoothnessReporting(
      base::ReadOnlySharedMemoryRegion& shared_memory);

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* message) override;
#endif

  RenderFrame* render_frame() const;

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  int routing_id() const { return routing_id_; }
#endif

 protected:
  explicit RenderFrameObserver(RenderFrame* render_frame);
#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  ~RenderFrameObserver() override;
#else
  virtual ~RenderFrameObserver();
#endif

 private:
  friend class RenderFrameImpl;

  // This is called by the RenderFrame when it's going away so that this object
  // can null out its pointer.
  void RenderFrameGone();

  raw_ptr<RenderFrame> render_frame_;

#if BUILDFLAG(CONTENT_ENABLE_LEGACY_IPC)
  // The routing ID of the associated RenderFrame.
  int routing_id_ = MSG_ROUTING_NONE;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
