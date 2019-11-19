// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_CONTROL_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_CONTROL_H_

#include <memory>

#include "base/callback.h"
#include "base/unguessable_token.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace blink {

class WebURL;
struct WebURLError;
class WebHistoryItem;
struct WebNavigationInfo;
struct WebNavigationParams;

// This interface gives control to navigation-related functionality of
// WebLocalFrame. It is separated from WebLocalFrame to give precise control
// over callers of navigation methods.
// WebLocalFrameClient gets a reference to this interface in BindToFrame.
class WebNavigationControl : public WebLocalFrame {
 public:
  ~WebNavigationControl() override {}

  // Runs beforeunload handlers for this frame and its local descendants.
  // Returns |true| if all the frames agreed to proceed with unloading
  // from their respective event handlers.
  // Note: this may lead to the destruction of the frame.
  virtual bool DispatchBeforeUnloadEvent(bool is_reload) = 0;

  // Commits a cross-document navigation in the frame. See WebNavigationParams
  // for details.
  // TODO(dgozman): return mojom::CommitResult.
  virtual void CommitNavigation(
      std::unique_ptr<WebNavigationParams> navigation_params,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data,
      base::OnceClosure call_before_attaching_new_document) = 0;

  // Commits a same-document navigation in the frame. For history navigations, a
  // valid WebHistoryItem should be provided. Returns CommitResult::Ok if the
  // navigation has actually committed.
  virtual mojom::CommitResult CommitSameDocumentNavigation(
      const WebURL&,
      WebFrameLoadType,
      const WebHistoryItem&,
      bool is_client_redirect,
      std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) = 0;

  // Loads a JavaScript URL in the frame.
  // TODO(dgozman): this may replace the document, so perhaps we should
  // return something meaningful?
  virtual void LoadJavaScriptURL(const WebURL&) = 0;

  enum FallbackContentResult {
    // An error page should be shown instead of fallback.
    NoFallbackContent,
    // Something else committed, no fallback content or error page needed.
    NoLoadInProgress,
    // Fallback content rendered, no error page needed.
    FallbackRendered
  };
  // On load failure, attempts to make frame's parent render fallback content.
  virtual FallbackContentResult MaybeRenderFallbackContent(
      const WebURLError&) const = 0;

  // When load failure is in a cross-process frame this notifies the frame here
  // that its owner should render fallback content if any. Only called on owners
  // that render their own content (i.e., <object>).
  virtual void RenderFallbackContent() const = 0;

  // Override the normal rules for whether a load has successfully committed
  // in this frame. Used to propagate state when this frame has navigated
  // cross process.
  virtual void SetCommittedFirstRealLoad() = 0;
  virtual bool HasCommittedFirstRealLoad() = 0;

  // Marks the frame as loading, before WebLocalFrameClient issues a navigation
  // request through the browser process on behalf of the frame.
  // This runs some JavaScript event listeners, which may cancel the navigation
  // or detach the frame. In this case the method returns false and client
  // should not proceed with the navigation.
  virtual bool WillStartNavigation(
      const WebNavigationInfo&,
      bool is_history_navigation_in_new_child_frame) = 0;

  // Informs the frame that the navigation it asked the client to do was
  // dropped.
  virtual void DidDropNavigation() = 0;

  // Marks the frame as loading, without performing any loading. Used for
  // initial history navigations in child frames, which may actually happen
  // in another process.
  virtual void MarkAsLoading() = 0;

  // TODO(ahemery): Remove all IsClientNavigationInitialHistoryLoad functions
  // when IsPerNavigationMojoInterface is enabled.
  virtual bool IsClientNavigationInitialHistoryLoad() = 0;

 protected:
  explicit WebNavigationControl(WebTreeScopeType scope)
      : WebLocalFrame(scope) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_CONTROL_H_
