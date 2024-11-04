// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/partitioned_popins_controller.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/page.h"

namespace content {

namespace {

// Closes the WebContents.
void CloseWebContents(base::WeakPtr<WebContents> web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (web_contents) {
    web_contents->ClosePage();
  }
}

// Closes the popin on a new task.
void SchedulePopinClose(WebContents* popin_web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!popin_web_contents) {
    return;
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CloseWebContents, popin_web_contents->GetWeakPtr()));
}

// Checks if the popin opener is a descendant of the provided render frame
// host, including when they are the same.
bool IsOpenerDescendantOfRenderFrameHost(RenderFrameHost* opener,
                                         RenderFrameHost* render_frame_host) {
  if (!opener || !render_frame_host) {
    return false;
  }
  for (RenderFrameHost* current = opener; current;
       current = current->GetParentOrOuterDocumentOrEmbedder()) {
    if (current == render_frame_host) {
      return true;
    }
  }
  return false;
}

WebContentsImpl* GetPopin(WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  return static_cast<WebContentsImpl*>(
      web_contents_impl->GetOpenedPartitionedPopin());
}

}  // namespace

WEB_CONTENTS_USER_DATA_KEY_IMPL(PartitionedPopinsController);

PartitionedPopinsController::PartitionedPopinsController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PartitionedPopinsController>(*web_contents) {
}

// Closes the popin if the opener's Render Frame is deleted.
void PartitionedPopinsController::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsImpl* popin = GetPopin(web_contents());
  if (popin &&
      render_frame_host == popin->GetPartitionedPopinOpener(PassKey())) {
    SchedulePopinClose(popin);
  }
}

// Closes the popin if the opener render frame host is changed.
void PartitionedPopinsController::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsImpl* popin = GetPopin(web_contents());
  if (popin && old_host == popin->GetPartitionedPopinOpener(PassKey())) {
    SchedulePopinClose(popin);
  }
}

// Closes the popin if the opener frame is deleted.
void PartitionedPopinsController::FrameDeleted(
    FrameTreeNodeId frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsImpl* popin = GetPopin(web_contents());
  if (popin && popin->GetPartitionedPopinOpener(PassKey()) ==
                   web_contents()->UnsafeFindFrameByFrameTreeNodeId(
                       frame_tree_node_id)) {
    SchedulePopinClose(popin);
  }
}

// Closes the popin if the opener frame navigates.
// TODO(crbug.com/375653317): Revisit if same-document navigation should be
// allowed. If same-document navigations are allowed then the ancestor checking
// done by `IsOpenerDescendantOfRenderFrameHost` below can be removed in favor
// of `FrameDeleted`.
void PartitionedPopinsController::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  WebContentsImpl* popin = GetPopin(web_contents());
  if (popin &&
      // Ancestor navigation is checked to close popins if the opener's ancestor
      // do a same-document navigation – e.g. a single-page application updates
      // the history – that is not handled by the `FrameDeleted` listener.
      IsOpenerDescendantOfRenderFrameHost(
          popin->GetPartitionedPopinOpener(PassKey()),
          navigation_handle->GetRenderFrameHost())) {
    SchedulePopinClose(popin);
  }
}

// Closes the popin if the WebContents is destroyed.
void PartitionedPopinsController::WebContentsDestroyed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsImpl* popin = GetPopin(web_contents());
  if (popin) {
    SchedulePopinClose(popin);
  }
}

// Closes the popin if the Page of the opener's WebContents changes.
void PartitionedPopinsController::PrimaryPageChanged(Page& page) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsImpl* popin = GetPopin(web_contents());
  if (popin) {
    SchedulePopinClose(popin);
  }
}

PartitionedPopinsController::~PartitionedPopinsController() = default;

}  // namespace content
