// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

#include <map>
#include <set>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

class DocumentOverlayWindow;
class OverlayWindow;
class WebContents;
class WebContentsImpl;
enum class PictureInPictureResult;

// DocumentPictureInPictureWindowControllerImpl handles Picture-in-Picture mode
// for HTML Document contents. It is very similar to video Picture-in-Picture
// mode, just using a WebContents view instead of a video element. See the
// content::PictureInPictureWindowControllerImpl documentation for additional
// context.
class CONTENT_EXPORT DocumentPictureInPictureWindowControllerImpl
    : public DocumentPictureInPictureWindowController,
      public WebContentsUserData<DocumentPictureInPictureWindowControllerImpl>,
      public WebContentsObserver {
 public:
  // Gets a reference to the controller associated with |web_contents| and
  // creates one if it does not exist. The returned pointer is guaranteed to be
  // non-null.
  static DocumentPictureInPictureWindowControllerImpl*
  GetOrCreateForWebContents(WebContents* web_contents);

  DocumentPictureInPictureWindowControllerImpl(
      const DocumentPictureInPictureWindowControllerImpl&) = delete;
  DocumentPictureInPictureWindowControllerImpl& operator=(
      const DocumentPictureInPictureWindowControllerImpl&) = delete;

  ~DocumentPictureInPictureWindowControllerImpl() override;

  // PictureInPictureWindowController:
  void Show() override;
  void FocusInitiator() override;
  void Close(bool should_pause_video) override;
  void CloseAndFocusInitiator() override;
  void OnWindowDestroyed(bool should_pause_video) override;
  WebContents* GetWebContents() override;

  // DocumentPictureInPictureWindowController:
  void SetChildWebContents(
      std::unique_ptr<WebContents> child_contents) override;
  WebContents* GetChildWebContents() override;
  DocumentOverlayWindow* GetWindowForTesting() override;

  // WebContentsObserver:
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(Page&) override;

 private:
  friend class WebContentsUserData<
      DocumentPictureInPictureWindowControllerImpl>;

  // To create an instance, use
  // DocumentPictureInPictureWindowControllerImpl::GetOrCreateForWebContents()
  explicit DocumentPictureInPictureWindowControllerImpl(
      WebContents* web_contents);

  // Signal to the media player that |this| is leaving Picture-in-Picture mode.
  // The should_pause_video argument signals the user's intent. If true, the
  // user explicitly closed the window and any active media should be paused.
  // If false, the user used a "return to tab" feature with the expectation
  // that any active media will continue playing in the parent tab.
  // TODO(klausw): connect this to the requestPictureInPicture API and/or
  // onleavepictureinpicture event once that's implemented.
  void OnLeavingPictureInPicture(bool should_pause_video);

  // Internal method to set the states after the window was closed, whether via
  // the system or by the browser.
  void CloseInternal(bool should_pause_video);

  // Returns the web_contents() as a WebContentsImpl*.
  WebContentsImpl* GetWebContentsImpl();

  void EnsureWindow();

  // Convenience routine for cases in which we'd like to cause the picture in
  // picture window to close. This will close the window, then clean up the
  // internal state.
  void ForceClosePictureInPicture();

  // If true, the PiP window is currently in the process of being closed.
  bool closing_ = false;

  // The controller owns both the overlay window and the child web contents
  // shown within the overlay window.
  std::unique_ptr<OverlayWindow> window_;
  std::unique_ptr<WebContents> child_contents_;

  class ChildContentsObserver : public WebContentsObserver {
   public:
    // Will call `close_cb` when `web_contents` navigates.
    ChildContentsObserver(WebContents* web_contents,
                          base::OnceClosure close_cb);
    ~ChildContentsObserver() override;

    // Check both `PrimaryPageChanged` and `DidStartNavigation`. We check
    // navigations immediately to fail as sooner, rather than fetch and then
    // fail on commit. We still check on commit as a fail-safe.
    void PrimaryPageChanged(Page&) override;
    void DidStartNavigation(NavigationHandle*) override;

   private:
    base::OnceClosure close_cb_;
  };

  // WebContentsObserver to watch for changes in `child_contents_`.
  std::unique_ptr<ChildContentsObserver> child_contents_observer_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<DocumentPictureInPictureWindowControllerImpl>
      weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
