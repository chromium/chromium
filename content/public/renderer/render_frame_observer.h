// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/byte_size.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/common/subresource_load_metrics.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/mojom/scroll/scroll_enums.mojom-shared.h"
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
}  // namespace blink

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace url {
class SchemeHostPort;
}  // namespace url

namespace content {

class RenderFrame;

// Base class for objects that want to filter incoming IPCs, and also get
// notified of changes to the frame.
class CONTENT_EXPORT RenderFrameObserver {
 public:
  RenderFrameObserver(const RenderFrameObserver&) = delete;
  RenderFrameObserver& operator=(const RenderFrameObserver&) = delete;

  // A subclass can use this to delete itself. If it does not, the subclass must
  // always null-check each call to render_frame() because the RenderFrame can
  // go away at any time.
  virtual void OnDestruct() = 0;

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
  virtual void DidDispatchDOMContentLoadedEvent() {}
  virtual void DidHandleOnloadEvents() {}
  virtual void DidCreateScriptContext(v8::Local<v8::Context> context,
                                      int32_t world_id) {}
  virtual void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                        int32_t world_id) {}
  virtual void DidClearWindowObject() {}
  virtual void DidChangeScrollOffset(blink::mojom::ScrollType) {}
  virtual void WillSubmitForm(const blink::WebFormElement& form) {}
  virtual void DidMatchCSS(
      const std::vector<blink::WebString>& newly_matching_selectors,
      const std::vector<blink::WebString>& stopped_matching_selectors) {}

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

  // Called when an interesting (from document lifecycle perspective),
  // compositor-driven layout had happened. This is a reasonable hook to use
  // to inspect the document and layout information, since it is in a clean
  // state and you won't accidentally force new layouts.
  // The interestingness of layouts is explained in WebMeaningfulLayout.h.
  virtual void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) {}

  // Notification when the renderer a response started, completed or canceled.
  // Complete or Cancel is guaranteed to be called for a response that started.
  // |request_id| uniquely identifies the request within this render frame.
  //
  // TODO(crbug.com/404425954): `DidStartResponse()`, `DidCompleteResponse()`,
  // `DidCancelResponse()` are going to be deprecated. Use callback setters in
  // `RenderFrame` instead.
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

  // Notification when the renderer observes data used during the page load.
  // This is used for page load metrics. |received_data_length| is the received
  // network bytes. |resource_id| uniquely identifies the resource within this
  // render frame.
  virtual void DidReceiveTransferSizeUpdate(
      int resource_id,
      base::ByteSize received_data_length) {}

  // Called when the focused element has changed to |element|.
  virtual void FocusedElementChanged(const blink::WebElement& element) {}

  // Called when accessibility is enabled or disabled.
  virtual void AccessibilityModeChanged(const ui::AXMode& mode) {}

  // Called when a worker fetch context will be created.
  virtual void WillCreateWorkerFetchContext(blink::WebWorkerFetchContext*) {}

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

  RenderFrame* render_frame() const;

  // This method exists for subclasses of Oilpan objects that need to clean up
  // the RenderFrameObserver in their pre-finalizer.
  void Dispose();

 protected:
  explicit RenderFrameObserver(RenderFrame* render_frame);
  virtual ~RenderFrameObserver();

 private:
  friend class RenderFrameImpl;

  // This is called by the RenderFrame when it's going away so that this object
  // can null out its pointer.
  void RenderFrameGone();

  raw_ptr<RenderFrame> render_frame_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FRAME_OBSERVER_H_
