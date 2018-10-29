// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NAVIGATION_STATE_H_
#define CONTENT_RENDERER_NAVIGATION_STATE_H_

#include <string>

#include "base/macros.h"
#include "content/common/frame.mojom.h"
#include "content/common/navigation_params.h"
#include "content/renderer/navigation_client.h"
#include "third_party/blink/public/web/commit_result.mojom.h"

namespace blink {
class WebDocumentLoader;
}

namespace content {

class CONTENT_EXPORT NavigationState {
 public:
  ~NavigationState();

  static std::unique_ptr<NavigationState> CreateBrowserInitiated(
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params,
      base::TimeTicks time_commit_requested,
      mojom::FrameNavigationControl::CommitNavigationCallback callback);

  static std::unique_ptr<NavigationState> CreateContentInitiated();

  static NavigationState* FromDocumentLoader(
      blink::WebDocumentLoader* document_loader);

  // True iff the frame's navigation was within the same document.
  bool WasWithinSameDocument();

  // True if this navigation was not initiated via WebFrame::LoadRequest.
  bool IsContentInitiated();

  const CommonNavigationParams& common_params() const { return common_params_; }
  const RequestNavigationParams& request_params() const {
    return request_params_;
  }
  bool request_committed() const { return request_committed_; }
  void set_request_committed(bool value) { request_committed_ = value; }
  void set_was_within_same_document(bool value) {
    was_within_same_document_ = value;
  }

  void set_transition_type(ui::PageTransition transition) {
    common_params_.transition = transition;
  }

  base::TimeTicks time_commit_requested() const {
    return time_commit_requested_;
  }

  // Only used when PerNavigationMojoInterface is enabled.
  void set_navigation_client(
      std::unique_ptr<NavigationClient> navigation_client_impl) {
    navigation_client_ = std::move(navigation_client_impl);
  }

  void set_navigation_start(const base::TimeTicks& navigation_start) {
    common_params_.navigation_start = navigation_start;
  }

  void RunCommitNavigationCallback(blink::mojom::CommitResult result);

 private:
  NavigationState(
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params,
      base::TimeTicks time_commit_requested,
      bool is_content_initiated,
      content::mojom::FrameNavigationControl::CommitNavigationCallback
          callback);

  bool request_committed_;
  bool was_within_same_document_;

  // True if this navigation was not initiated via WebFrame::LoadRequest.
  const bool is_content_initiated_;

  CommonNavigationParams common_params_;

  // Note: if IsContentInitiated() is false, whether this navigation should
  // replace the current entry in the back/forward history list is determined by
  // the should_replace_current_entry field in |history_params|. Otherwise, use
  // replacesCurrentHistoryItem() on the WebDataSource.
  //
  // TODO(davidben): It would be good to unify these and have only one source
  // for the two cases. We can plumb this through WebFrame::loadRequest to set
  // lockBackForwardList on the FrameLoadRequest. However, this breaks process
  // swaps because FrameLoader::loadWithNavigationAction treats loads before a
  // FrameLoader has committedFirstRealDocumentLoad as a replacement. (Added for
  // http://crbug.com/178380).
  const RequestNavigationParams request_params_;

  // Time when RenderFrameImpl::CommitNavigation() is called.
  base::TimeTicks time_commit_requested_;

  // The NavigationClient interface gives control over the navigation ongoing in
  // the browser process.
  // Only used when PerNavigationMojoInterface is enabled.
  std::unique_ptr<NavigationClient> navigation_client_;

  // Used to notify whether a commit request from the browser process was
  // successful or not.
  mojom::FrameNavigationControl::CommitNavigationCallback commit_callback_;

  DISALLOW_COPY_AND_ASSIGN(NavigationState);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NAVIGATION_STATE_H_
