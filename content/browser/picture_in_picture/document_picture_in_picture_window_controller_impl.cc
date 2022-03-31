// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/document_picture_in_picture_window_controller_impl.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/picture_in_picture/picture_in_picture_session.h"
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
  // The controller must not have pre-existing web content. It's supposed
  // to have been destroyed by CloseInternal() if it's being reused.
  DCHECK(!controller->GetChildWebContents());
  return controller;
}
DocumentPictureInPictureWindowControllerImpl::
    DocumentPictureInPictureWindowControllerImpl(WebContents* web_contents)
    : WebContentsUserData<DocumentPictureInPictureWindowControllerImpl>(
          *web_contents),
      WebContentsObserver(web_contents) {}

DocumentPictureInPictureWindowControllerImpl::
    ~DocumentPictureInPictureWindowControllerImpl() = default;

void DocumentPictureInPictureWindowControllerImpl::SetChildWebContents(
    std::unique_ptr<WebContents> child_contents) {
  // This method should only be called once for a given controller.
  DCHECK(!child_contents_);
  child_contents_ = std::move(child_contents);
  child_contents_observer_ = std::make_unique<ChildContentsObserver>(
      GetChildWebContents(),
      base::BindOnce(&DocumentPictureInPictureWindowControllerImpl::
                         ForceClosePictureInPicture,
                     weak_factory_.GetWeakPtr()));
}

WebContents*
DocumentPictureInPictureWindowControllerImpl::GetChildWebContents() {
  return child_contents_.get();
}

void DocumentPictureInPictureWindowControllerImpl::Show() {
  closing_ = false;

  EnsureWindow();
  window_->ShowInactive();
  GetWebContentsImpl()->SetHasPictureInPictureDocument(true);
}

void DocumentPictureInPictureWindowControllerImpl::EnsureWindow() {
  if (window_)
    return;

  window_ =
      GetContentClient()->browser()->CreateWindowForDocumentPictureInPicture(
          this);
}

void DocumentPictureInPictureWindowControllerImpl::FocusInitiator() {
  GetWebContentsImpl()->Activate();
}

void DocumentPictureInPictureWindowControllerImpl::Close(
    bool should_pause_video) {
  if (!window_ || closing_)
    return;

  closing_ = true;
  window_->Close();

  CloseInternal(should_pause_video);
}

void DocumentPictureInPictureWindowControllerImpl::CloseAndFocusInitiator() {
  Close(false /* should_pause_video */);
  FocusInitiator();
}

void DocumentPictureInPictureWindowControllerImpl::OnWindowDestroyed(
    bool should_pause_video) {
  window_ = nullptr;
  CloseInternal(should_pause_video);
}

WebContents* DocumentPictureInPictureWindowControllerImpl::GetWebContents() {
  return web_contents();
}

void DocumentPictureInPictureWindowControllerImpl::WebContentsDestroyed() {
  ForceClosePictureInPicture();
}

void DocumentPictureInPictureWindowControllerImpl::PrimaryPageChanged(Page&) {
  ForceClosePictureInPicture();
}

void DocumentPictureInPictureWindowControllerImpl::OnLeavingPictureInPicture(
    bool should_pause_video) {
  // TODO(klausw): this method should be called when the parent web contents are
  // about to be destroyed, but that currently doesn't seem to be happening. In
  // any case, OnWindowDestroyed will do cleanup even when
  // OnLeavingPictureInPicture didn't get triggered.
  GetWebContentsImpl()->ExitPictureInPicture();
}

void DocumentPictureInPictureWindowControllerImpl::
    ForceClosePictureInPicture() {
  if (window_)
    window_->Close();
  CloseInternal(/*should_pause_video=*/true);
}

void DocumentPictureInPictureWindowControllerImpl::CloseInternal(
    bool should_pause_video) {
  // Avoid issues in case `CloseInternal()` gets called twice.
  // See PictureInPictureWindowControllerImpl::CloseInternal
  if (!window_ || web_contents()->IsBeingDestroyed()) {
    // Ensure the child web contents are destroyed even in case a previous
    // destruction was incomplete.
    child_contents_ = nullptr;
    return;
  }

  GetWebContentsImpl()->SetHasPictureInPictureDocument(false);
  OnLeavingPictureInPicture(should_pause_video);
  window_ = nullptr;
  child_contents_ = nullptr;
}

WebContentsImpl*
DocumentPictureInPictureWindowControllerImpl::GetWebContentsImpl() {
  return static_cast<WebContentsImpl*>(web_contents());
}

DocumentOverlayWindow*
DocumentPictureInPictureWindowControllerImpl::GetWindowForTesting() {
  return static_cast<DocumentOverlayWindow*>(window_.get());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DocumentPictureInPictureWindowControllerImpl);

DocumentPictureInPictureWindowControllerImpl::ChildContentsObserver::
    ChildContentsObserver(WebContents* web_contents, base::OnceClosure close_cb)
    : WebContentsObserver(web_contents), close_cb_(std::move(close_cb)) {}

DocumentPictureInPictureWindowControllerImpl::ChildContentsObserver::
    ~ChildContentsObserver() = default;

void DocumentPictureInPictureWindowControllerImpl::ChildContentsObserver::
    PrimaryPageChanged(Page&) {
  // If we've already tried to close the window, then there's nothing to do.
  if (!close_cb_)
    return;

  // Don't run `close_cb` from within the observer, since closing `web_contents`
  // is not allowed during an observer callback.
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(close_cb_));
}

void DocumentPictureInPictureWindowControllerImpl::ChildContentsObserver::
    DidStartNavigation(NavigationHandle* navigation_handle) {
  // If we've already tried to close the window, then there's nothing to do.
  if (!close_cb_)
    return;

  // Only care if it's the root of the pip window.
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  // History / etc. navigations are okay.
  if (navigation_handle->IsSameDocument())
    return;

  // about::blank is okay, since that's what it starts with.
  if (navigation_handle->GetURL().IsAboutBlank())
    return;

  // Don't run `close_cb` from within the observer, since closing `web_contents`
  // is not allowed during an observer callback.
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(close_cb_));
}

}  // namespace content
