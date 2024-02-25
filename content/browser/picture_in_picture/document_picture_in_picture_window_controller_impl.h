// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

#include <map>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

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
  std::optional<gfx::Rect> GetWindowBounds() override;
  WebContents* GetChildWebContents() override;
  std::optional<url::Origin> GetOrigin() override;

  // DocumentPictureInPictureWindowController:
  void SetChildWebContents(WebContents* child_contents) override;

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

  // Internal method to set the states after the pip has been stopped, whether
  // via the page or by the browser.  Notifies the opener that pip has ended.
  // This is the only thing that should clear `child_contents_`, and it should
  // always clear `child_contents_`.
  void NotifyClosedAndStopObserving(bool should_pause_video);

  // Called when the child WebContents discovers that it's being deleted.
  void OnChildContentsDestroyed();

  // Returns the web_contents() as a WebContentsImpl*.
  WebContentsImpl* GetWebContentsImpl();

  // The WebContents for the PiP window. If this is null, then we have already
  // closed / stopped Picture in Picture.
  raw_ptr<WebContents> child_contents_ = nullptr;

  class ChildContentsObserver : public WebContentsObserver {
   public:
    // Will post `force_close_cb` when `web_contents` navigates, or at similar
    // times when the PiP session should end. `contents_destroyed_cb` will be
    // called in-line (not posted) when our WebContents has been destroyed and
    // the pointer should be discarded.
    ChildContentsObserver(WebContents* web_contents,
                          base::OnceClosure force_close_cb,
                          base::OnceClosure contents_destroyed_cb);
    ~ChildContentsObserver() override;

    // Watch for navigations in the child contents, so that we can close the PiP
    // window if it navigates away.  Some navigations (e.g., same-document) are
    // allowed here.
    void DidStartNavigation(NavigationHandle*) override;

    // If the PiP window is destroyed, notify the opener.
    void WebContentsDestroyed() override;

    // The PiP window should never be duplicated.
    void DidCloneToNewWebContents(WebContents*, WebContents*) override;

   private:
    // Called, via post, to request that the pip session end.
    base::OnceClosure force_close_cb_;

    // Called, without posting, when the raw ptr to our WebContents is about to
    // be invalidated.
    base::OnceClosure contents_destroyed_cb_;
  };

  raw_ptr<WebContents> opener_web_contents_ = nullptr;

  // WebContentsObserver to watch for changes in `child_contents_`.
  std::unique_ptr<ChildContentsObserver> child_contents_observer_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtrFactory<DocumentPictureInPictureWindowControllerImpl>
      weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_DOCUMENT_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
