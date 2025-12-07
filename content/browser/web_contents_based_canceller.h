// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_BASED_CANCELLER_H_
#define CONTENT_BROWSER_WEB_CONTENTS_BASED_CANCELLER_H_

#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class RenderFrameHost;

// A helper class to make it easy to react to RenderFrameHosts being or becoming
// inactive and WebContents becoming invisible. Typical usage is for dealing
// with the navigation/tab races that occur when opening a dialog. These can
// lead to dialogs remaining open on top of the wrong tab or while their content
// is inactive.
//
// To use this, first attempt to create it with `Create` if this returns
// `nullptr` then the cancel condition is already true and you should not
// proceed. After that, ensure that the returned object lasts for the lifetime
// of the dialog and call `SetCancelCallback` to correctly react to events.
//
// Exported for testing.
class CONTENT_EXPORT WebContentsBasedCanceller : public WebContentsObserver {
  using CancelCallback = base::OnceCallback<void()>;

 public:
  // Specifies what conditions to pay attention to when deciding whether we
  // should proceed or cancel.
  enum class CancelCondition {
    // Cancel if `RenderFrameHost::IsActive` returns false. Use this for
    // something that is incompatible with back/forward-cache, e.g. screen
    // capture should not occur while in back/forward-cache.
    kActiveState = 0,
    // Cancel if `WebContents::GetVisibility` is `HIDDEN` or the WebContents is
    // not the active tab in the browser. Use this for something that is
    // incompatible with switching tabs, e.g. a modal system dialog like a
    // file-chooser that displays on top of everything should be dismissed if
    // the tab is no longer the visible tab. This also implies `kActiveState`
    // because an inactive RFH won't be visible.
    kVisibility = 1,
  };
  // If the cancel condition is already true, this returns `nullptr`. Otherwise
  // it returns an instance of `WebContentsBasedCanceller`.
  static std::unique_ptr<WebContentsBasedCanceller> Create(
      RenderFrameHost* rfh,
      CancelCondition condition);
  ~WebContentsBasedCanceller() override;

  // Set the callback to run when the condition becomes true. This is set after
  // construction because typically it references to something that doesn't
  // exist until after the `WebContentsBasedCanceller` is constructed.
  //
  // It is OK to call this later, in a different task. It will recheck the
  // conditions and possibly execute the callback immediately.
  void SetCancelCallback(CancelCallback cancel_callback);

 private:
  WebContentsBasedCanceller(RenderFrameHost* render_frame_host,
                            CancelCondition condition);
  bool CanShow();
  bool CanShowForVisibility(Visibility visibility);
  bool CanShowForRFHActiveState();
  bool CanShowForTabState();

  // WebContentsObserver
  void OnVisibilityChanged(Visibility visibility) override;
  void RenderFrameHostStateChanged(
      RenderFrameHost* render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  CancelCondition condition_;
  WeakDocumentPtr document_;
  CancelCallback cancel_callback_;
};

}  // namespace content
#endif  // CONTENT_BROWSER_WEB_CONTENTS_BASED_CANCELLER_H_
