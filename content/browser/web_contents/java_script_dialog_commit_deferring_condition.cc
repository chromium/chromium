// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/java_script_dialog_commit_deferring_condition.h"

#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

// static
std::unique_ptr<CommitDeferringCondition>
JavaScriptDialogCommitDeferringCondition::MaybeCreate(
    NavigationRequest& navigation_request) {
  DCHECK(navigation_request.GetWebContents());
  auto& web_contents =
      static_cast<WebContentsImpl&>(*navigation_request.GetWebContents());
  if (!web_contents.JavaScriptDialogDefersNavigations())
    return nullptr;

  if (navigation_request.IsInMainFrame()) {
    // A dialog should not defer navigations in the non-primary main frame (e.g.
    // prerendering).
    if (!navigation_request.IsInPrimaryMainFrame())
      return nullptr;
  } else {
    // Don't defer navigations that occur in a prerendering subframe since
    // prerendered pages can't show dialogs.
    if (navigation_request.frame_tree_node()->parent()->GetLifecycleState() ==
        RenderFrameHost::LifecycleState::kPrerendering)
      return nullptr;
  }

  bool user_navigation = navigation_request.IsInMainFrame() &&
                         (!navigation_request.IsRendererInitiated() ||
                          navigation_request.HasUserGesture());

  // Don't prevent the user from navigating away from the page.
  // Don't defer downloads, which don't leave the page.
  if (user_navigation || navigation_request.IsDownload())
    return nullptr;

  return base::WrapUnique(
      new JavaScriptDialogCommitDeferringCondition(navigation_request));
}

JavaScriptDialogCommitDeferringCondition::
    JavaScriptDialogCommitDeferringCondition(NavigationRequest& request)
    : CommitDeferringCondition(request) {}

JavaScriptDialogCommitDeferringCondition::
    ~JavaScriptDialogCommitDeferringCondition() = default;

CommitDeferringCondition::Result
JavaScriptDialogCommitDeferringCondition::WillCommitNavigation(
    base::OnceClosure resume) {
  auto* web_contents =
      static_cast<WebContentsImpl*>(GetNavigationHandle().GetWebContents());
  DCHECK(web_contents);

  // It's possible that, depending on the order deferrals are run, the dialog
  // may have been dismissed by the time we run this check. If that's the
  // case, move on synchronously to the next deferral.
  if (!web_contents->JavaScriptDialogDefersNavigations())
    return Result::kProceed;

  web_contents->NotifyOnJavaScriptDialogDismiss(std::move(resume));
  return Result::kDefer;
}

}  // namespace content
