// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NAVIGATION_STATE_H_
#define CONTENT_RENDERER_NAVIGATION_STATE_H_

#include <memory>

#include "base/macros.h"
#include "content/common/frame.mojom.h"
#include "content/common/navigation_params.h"
#include "content/common/navigation_params.mojom.h"
#include "content/renderer/navigation_client.h"

namespace blink {
class WebDocumentLoader;

namespace mojom {
enum class CommitResult;
}
}

namespace content {

class CONTENT_EXPORT NavigationState {
 public:
  ~NavigationState();

  static std::unique_ptr<NavigationState> Create(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      mojom::NavigationClient::CommitNavigationCallback
          per_navigation_mojo_interface_callback,
      std::unique_ptr<NavigationClient> navigation_client,
      bool was_initiated_in_this_frame);

  static std::unique_ptr<NavigationState> CreateForSynchronousCommit();

  static NavigationState* FromDocumentLoader(
      blink::WebDocumentLoader* document_loader);

  // True iff the frame's navigation was within the same document.
  bool WasWithinSameDocument();

  bool IsForSynchronousCommit();

  const mojom::CommonNavigationParams& common_params() const {
    return *common_params_;
  }
  const mojom::CommitNavigationParams& commit_params() const {
    return *commit_params_;
  }
  bool has_navigation_client() const { return navigation_client_.get(); }
  void set_was_within_same_document(bool value) {
    was_within_same_document_ = value;
  }

  bool was_initiated_in_this_frame() const {
    return was_initiated_in_this_frame_;
  }

  void set_transition_type(ui::PageTransition transition) {
    common_params_->transition = transition;
  }

  void RunCommitNavigationCallback(
      mojom::DidCommitProvisionalLoadParamsPtr params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params);

 private:
  NavigationState(mojom::CommonNavigationParamsPtr common_params,
                  mojom::CommitNavigationParamsPtr commit_params,
                  bool is_for_synchronous_commit,
                  content::mojom::NavigationClient::CommitNavigationCallback
                      commit_callback,
                  std::unique_ptr<NavigationClient> navigation_client,
                  bool was_initiated_in_this_frame);

  bool was_within_same_document_;

  // Indicates whether the navigation was initiated by the same RenderFrame
  // it is about to commit in. An example would be a link click.
  // A counter-example would be user typing in the url bar (browser-initiated
  // navigation), or a link click leading to a process swap (different
  // RenderFrame instance).
  // Used to ensure consistent observer notifications about a navigation.
  bool was_initiated_in_this_frame_;

  // True if this navigation is for a renderer synchronous commit (e.g. the
  // synchronous about:blank navigation, same-origin initiated same-document
  // navigations), rather than using the browser's navigation stack.
  const bool is_for_synchronous_commit_;

  mojom::CommonNavigationParamsPtr common_params_;

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
  mojom::CommitNavigationParamsPtr commit_params_;

  // The NavigationClient interface gives control over the navigation ongoing in
  // the browser process.
  std::unique_ptr<NavigationClient> navigation_client_;

  // Used to notify whether a commit request from the browser process was
  // successful or not.
  mojom::NavigationClient::CommitNavigationCallback commit_callback_;

  DISALLOW_COPY_AND_ASSIGN(NavigationState);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NAVIGATION_STATE_H_
