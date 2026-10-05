// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_BASED_CANCELLER_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_BASED_CANCELLER_H_

#include <memory>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

namespace content {

// This class is used to cancel an operation (such as showing a dialog) when
// the WebContents becomes unsuitable for showing the UI.
//
// Example usage:
//   auto canceller = WebContentsBasedCanceller::Create(rfh, condition);
//   if (!canceller) {
//     // Already unsuitable, abort immediately.
//     return;
//   }
//   canceller->SetCancelCallback(base::BindOnce(&MyDialog::Cancel, ...));
class CONTENT_EXPORT WebContentsBasedCanceller : public WebContentsObserver {
  using CancelCallback = base::OnceCallback<void()>;

 public:
  // Returns the size of the top-level window containing `web_contents`.
  static gfx::Size GetTopLevelWindowSize(WebContents* web_contents);

  // Returns true if the top-level window containing `web_contents` is smaller
  // than `min_window_size` in either dimension, unless `web_contents` is on an
  // exempt scheme.
  static bool IsWindowTooSmall(WebContents* web_contents,
                               const gfx::Size& min_window_size);

  // Specifies what conditions to pay attention to when deciding whether we
  // should proceed or cancel.
  enum class CancelCondition {
    // Only require the RFH to be active.
    kActiveState,
    // In addition to the above, require the WebContents to be visible (not
    // hidden or occluded), in an active tab, and the top-level window not
    // smaller than `min_window_size`.
    kVisibility,
    // Only require the top-level window not to be smaller than
    // `min_window_size`.
    kWindowSize,
  };

  // Checks if `rfh` currently satisfies `condition`. If not, returns nullptr.
  // Otherwise, starts observing the WebContents and when `condition` is no
  // longer satisfied, calls the callback provided to `SetCancelCallback`. If
  // `SetCancelCallback` has not been called yet, it will be called as soon as
  // `SetCancelCallback` is called.
  //
  // We split `Create` and `SetCancelCallback` so that `Create` can be called
  // before the object that will be cancelled is constructed. If `Create`
  // returns nullptr, we can avoid constructing the object at all. If it
  // returns an instance of `WebContentsBasedCanceller`, we can pass that
  // instance to the object's constructor and the object can call
  // `SetCancelCallback`.
  static std::unique_ptr<WebContentsBasedCanceller> Create(
      RenderFrameHost* rfh,
      CancelCondition condition,
      gfx::Size min_window_size = gfx::Size());

  ~WebContentsBasedCanceller() override;

  // Sets the callback to be called when the condition is no longer satisfied.
  // This can only be called once. It is separate from `Create` because the
  // object to be cancelled may not exist until after the
  // `WebContentsBasedCanceller` is constructed.
  void SetCancelCallback(CancelCallback cancel_callback);

  // Returns true if all required conditions currently hold.
  bool CanShow();

 private:
  WebContentsBasedCanceller(RenderFrameHost* render_frame_host,
                            CancelCondition condition,
                            gfx::Size min_window_size);

  bool CanShowForVisibility(Visibility visibility);
  bool CanShowForRFHActiveState();
  bool CanShowForTabState();
  bool CanShowForSize();

  // WebContentsObserver
  void OnVisibilityChanged(Visibility visibility) override;
  void RenderFrameHostStateChanged(
      RenderFrameHost* changed_render_frame_host,
      RenderFrameHost::LifecycleState old_state,
      RenderFrameHost::LifecycleState new_state) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void PrimaryMainFrameWasResized(bool width_changed) override;
  void FrameSizeChanged(RenderFrameHost* render_frame_host,
                        const gfx::Size& new_size) override;

  CancelCondition condition_;
  gfx::Size min_window_size_;
  WeakDocumentPtr document_;
  CancelCallback cancel_callback_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_BASED_CANCELLER_H_
