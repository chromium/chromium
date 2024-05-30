// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NO_VARY_SEARCH_HINT_COMMIT_DEFERRING_CONDITION_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NO_VARY_SEARCH_HINT_COMMIT_DEFERRING_CONDITION_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/browser/renderer_host/navigation_type.h"
#include "content/public/browser/commit_deferring_condition.h"

namespace content {

class NavigationRequest;

// This CommitDeferringCondition waits until the prerender navigation receives
// the main page's headers from the server. After receiving the headers,
// we can check if the associated PrerenderHost's initial URL matches via
// No-Vary-Search header in the subsequent CommitDeferringConditions.
class PrerenderNoVarySearchHintCommitDeferringCondition
    : public CommitDeferringCondition,
      public PrerenderHost::Observer {
 public:
  ~PrerenderNoVarySearchHintCommitDeferringCondition() override;
  static std::unique_ptr<CommitDeferringCondition> MaybeCreate(
      NavigationRequest& navigation_request,
      NavigationType navigation_type,
      std::optional<int> candidate_prerender_frame_tree_node_id);
  Result WillCommitNavigation(base::OnceClosure resume) override;

 private:
  PrerenderNoVarySearchHintCommitDeferringCondition(
      NavigationRequest& navigation_request,
      int candidate_prerender_frame_tree_node_id);
  // PrerenderHost::Observer
  void OnHeadersReceived() override;
  void OnHostDestroyed(PrerenderFinalStatus status) override;

  const int candidate_prerender_frame_tree_node_id_;
  base::OnceClosure resume_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_NO_VARY_SEARCH_HINT_COMMIT_DEFERRING_CONDITION_H_
