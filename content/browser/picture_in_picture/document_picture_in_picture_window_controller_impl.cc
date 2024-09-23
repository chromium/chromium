// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/document_picture_in_picture_window_controller_impl.h"

#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_session.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"

namespace content {

// static
DocumentPictureInPictureWindowController*
PictureInPictureWindowController::GetOrCreateDocumentPictureInPictureController(
    WebContents* web_contents) {
  return DocumentPictureInPictureWindowControllerImpl::
      GetOrCreateForWebContents(web_contents);
}

// static
DocumentPictureInPictureWindowControllerImpl*
DocumentPictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);

  // This is a no-op if the controller already exists.
  CreateForWebContents(web_contents);
  auto* controller = FromWebContents(web_contents);
  return controller;
}

DocumentPictureInPictureWindowControllerImpl::
    DocumentPictureInPictureWindowControllerImpl(WebContents* web_contents)
    : WebContentsUserData<DocumentPictureInPictureWindowControllerImpl>(
          *web_contents),
      opener_web_contents_(web_contents) {}

DocumentPictureInPictureWindowControllerImpl::
    ~DocumentPictureInPictureWindowControllerImpl() = default;

void DocumentPictureInPictureWindowControllerImpl::SetChildWebContents(
    WebContents* child_contents) {
  // This method should only be called once for a given controller.
  DCHECK(!child_contents_);
  child_contents_ = child_contents;
  // Start observing immediately, so that we don't miss a destruction event.
  child_contents_observer_ = std::make_unique<ChildContentsObserver>(
      GetChildWebContents(),
      base::BindOnce(&DocumentPictureInPictureWindowControllerImpl::Close,
                     weak_factory_.GetWeakPtr(), /*should_pause_video=*/true),
      base::BindOnce(&DocumentPictureInPictureWindowControllerImpl::
                         OnChildContentsDestroyed,
                     weak_factory_.GetWeakPtr()));
}

WebContents*
DocumentPictureInPictureWindowControllerImpl::GetChildWebContents() {
  return child_contents_;
}

void DocumentPictureInPictureWindowControllerImpl::Show() {
  // It would nice if we were provided with the child WebContents, but this
  // method is shared with non-WebContents video PiP. So, we just have to be
  // confident that somebody has set it already.
  DCHECK(child_contents_);

  // Start observing our WebContents. Note that this is safe, since we're
  // owned by the opener WebContents.
  Observe(opener_web_contents_);

  // We're shown automatically by the browser that runs the Picture in Picture
  // window, so nothing needs to happen for the window to show up.
  GetWebContentsImpl()->SetHasPictureInPictureDocument(true);
}

void DocumentPictureInPictureWindowControllerImpl::FocusInitiator() {
  GetWebContentsImpl()->Activate();
}

void DocumentPictureInPictureWindowControllerImpl::Close(
    bool should_pause_video) {
  if (!child_contents_)
    return;

  child_contents_->ClosePage();

  NotifyClosedAndStopObserving(should_pause_video);
  // Since we use `child_contents_` to gate everything, make sure it's null.
  DCHECK(!child_contents_);
}

void DocumentPictureInPictureWindowControllerImpl::CloseAndFocusInitiator() {
  FocusInitiator();
  Close(false /* should_pause_video */);
}

void DocumentPictureInPictureWindowControllerImpl::OnWindowDestroyed(
    bool should_pause_video) {
  // We instead watch for the WebContents.
  NOTREACHED_IN_MIGRATION();
}

WebContents* DocumentPictureInPictureWindowControllerImpl::GetWebContents() {
  return web_contents();
}

std::optional<url::Origin>
DocumentPictureInPictureWindowControllerImpl::GetOrigin() {
  return std::nullopt;
}

void DocumentPictureInPictureWindowControllerImpl::WebContentsDestroyed() {
  // The opener web contents are being destroyed. Stop observing, and forget
  // `opener_web_contents_`. This will also prevent `NotifyAndStopObserving`
  // from trying to send messages to the opener, which is not safe during
  // teardown.
  Observe(/*web_contents=*/nullptr);
  opener_web_contents_ = nullptr;
  Close(/*should_pause_video=*/true);
}

std::optional<gfx::Rect>
DocumentPictureInPictureWindowControllerImpl::GetWindowBounds() {
  if (!child_contents_)
    return std::nullopt;
  return child_contents_->GetContainerBounds();
}

void DocumentPictureInPictureWindowControllerImpl::PrimaryPageChanged(Page&) {
  Close(/*should_pause_video=*/true);
}

void DocumentPictureInPictureWindowControllerImpl::NotifyClosedAndStopObserving(
    bool should_pause_video) {
  // Do not ask `child_contents_` to close itself; we're called both when the
  // opener wants to close and when the child wants to / is in the process of
  // closing. In particular, we might be called synchronously from a
  // WebContentsObserver. It's okay to unregister, though, since that's
  // explicitly allowed by WebContentsObserver even during callbacks. Calling
  // ClosePage is not be a good idea.

  // Avoid issues in case `NotifyClosedAndStopObserving()` gets called twice.
  // If we already have no child contents, then there's nothing to do. This is
  // the only thing that clears it, so it's already been run.
  if (!child_contents_)
    return;

  // Forget about the child contents. Nothing else should clear
  // `child_contents_`; all cleanup should end up going through here.
  child_contents_ = nullptr;
  child_contents_observer_.reset();

  // If the opener is being destroyed, then don't dispatch anything to it.
  if (!GetWebContentsImpl())
    return;

  // Notify the opener, and stop observing it.
  GetWebContentsImpl()->SetHasPictureInPictureDocument(false);
  // Signal to the media player that |this| is leaving Picture-in-Picture mode.
  // The should_pause_video argument signals the user's intent. If true, the
  // user explicitly closed the window and any active media should be paused.
  // If false, the user used a "return to tab" feature with the expectation
  // that any active media will continue playing in the parent tab.
  // TODO(crbug.com/40877557): connect this to the requestPictureInPicture
  // API and/or onleavepictureinpicture event once that's implemented.
  GetWebContentsImpl()->ExitPictureInPicture();
  Observe(/*web_contents=*/nullptr);
}

void DocumentPictureInPictureWindowControllerImpl::OnChildContentsDestroyed() {
  NotifyClosedAndStopObserving(true);
  // Make extra sure that the raw pointer is cleared, to avoid UAF.
  CHECK(!child_contents_);
}

WebContentsImpl*
DocumentPictureInPictureWindowControllerImpl::GetWebContentsImpl() {
  return static_cast<WebContentsImpl*>(web_contents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DocumentPictureInPictureWindowControllerImpl);

DocumentPictureInPictureWindowControllerImpl::ChildContentsObserver::
    ChildContentsObserver(WebContents* web_contents,
                          base::OnceClosure force_close_cb,
                          base::OnceClosure contents_destroyed_cb)
    : WebContentsObserver(web_contents),
      force_close_cb_(std::move(force_close_cb)),
      contents_destroyed_cb_(std::move(contents_destroyed_cb)) {}

DocumentPictureInPictureWindowControllerImpl::ChildContentsObserver::
    ~ChildContentsObserver() = default;

void DocumentPictureInPictureWindowControllerImpl::ChildContentsObserver::
    DidStartNavigation(NavigationHandle* navigation_handle) {
  // If we've already tried to close the window, then there's nothing to do.
  if (!force_close_cb_) {
    return;
  }

  // Only care if it's the root of the pip window.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // History / etc. navigations are okay.
  if (navigation_handle->IsSameDocument()) {
    return;
  }

  // We allow the synchronous about:blank commit to succeed, since that is part
  // of most initial navigations. Subsequent navigations to about:blank are
  // treated like other navigations and close the window.
  // `is_synchronous_renderer_commit()` will only be true for the initial
  // about:blank navigation.
  if (navigation_handle->GetURL().IsAboutBlank() &&
      NavigationRequest::From(navigation_handle)
          ->is_synchronous_renderer_commit()) {
    return;
  }

  // Don't run `force_close_cb` from within the observer, since closing
  // `web_contents` is not allowed during an observer callback.
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(force_close_cb_));
}

void DocumentPictureInPictureWindowControllerImpl::ChildContentsObserver::
    WebContentsDestroyed() {
  // Notify immediately that the child contents have been destroyed -- do not
  // post, else something could reference the raw ptr.
  if (contents_destroyed_cb_)
    std::move(contents_destroyed_cb_).Run();
}

void DocumentPictureInPictureWindowControllerImpl::ChildContentsObserver::
    DidCloneToNewWebContents(WebContents*, WebContents*) {
  // DocumentPictureInPictureWindows should never be duplicated, since there
  // should only ever be one PiP window and the duplicated window bypasses some
  // of the controller logic here. This is a regression check for
  // https://crbug.com/1413919.
  NOTREACHED_IN_MIGRATION();
}

}  // namespace content
