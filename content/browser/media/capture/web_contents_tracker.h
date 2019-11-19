// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Given a starting render_process_id and main_render_frame_id, the
// WebContentsTracker tracks changes to the active RenderFrameHost tree during
// the lifetime of a WebContents instance. This is used to maintain capture of
// the WebContents's video and audio across page transitions such as user
// navigations, crashes, iframes, etc..
//
// Threading issues: Start(), Stop() and the ChangeCallback must be invoked on
// the same thread. This can be any thread, and the decision is locked-in once
// WebContentsTracker::Start() is called.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_TRACKER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_TRACKER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class RenderWidgetHostView;

class CONTENT_EXPORT WebContentsTracker
    : public base::RefCountedThreadSafe<WebContentsTracker>,
      public WebContentsObserver {
 public:
  WebContentsTracker();

  // Callback to indicate a new RenderWidgetHostView should be targeted for
  // capture. This is also invoked with false to indicate tracking will not
  // continue (i.e., the WebContents instance was not found or has been
  // destroyed).
  typedef base::Callback<void(bool was_still_tracking)> ChangeCallback;

  // Start tracking.  The last-known |render_process_id| and
  // |main_render_frame_id| are provided, and |callback| will be run once to
  // indicate whether tracking successfully started (this may occur during the
  // invocation of Start(), or in the future).  The callback will be invoked on
  // the same thread calling Start().
  virtual void Start(int render_process_id, int main_render_frame_id,
                     const ChangeCallback& callback);

  // Stop tracking.  Once this method returns, the callback is guaranteed not to
  // be invoked again.
  virtual void Stop();

  // Returns true if this tracker is still able to continue tracking changes.
  // This must only be called on the UI BrowserThread.
  bool is_still_tracking() const { return !!web_contents(); }

  // Current target view. May return nullptr during certain transient periods.
  // This must only be called on the UI BrowserThread.
  RenderWidgetHostView* GetTargetView() const;

  // Set a callback that is run whenever the main frame of the WebContents is
  // resized.  This method must be called on the same thread that calls
  // Start()/Stop(), and |callback| will be run on that same thread.  Calling
  // the Stop() method guarantees the callback will never be invoked again.
  void SetResizeChangeCallback(const base::Closure& callback);

 protected:
  friend class base::RefCountedThreadSafe<WebContentsTracker>;
  ~WebContentsTracker() override;

 private:
  // Determine the target RenderWidgetHostView and, if different from that last
  // reported, runs the ChangeCallback on the appropriate thread. If
  // |force_callback_run| is true, the ChangeCallback is run even if the
  // RenderWidgetHostView has not changed.
  void OnPossibleTargetChange(bool force_callback_run);

  // Called on the thread that Start()/Stop() are called on.  Checks whether the
  // callback is still valid and, if so, runs it.
  void MaybeDoCallback(bool was_still_tracking);

  // Called on the thread that Start()/Stop() are called on.  Checks whether the
  // callback is still valid and, if so, runs it to indicate the main frame has
  // changed in size.
  void MaybeDoResizeCallback();

  // Look-up the current WebContents instance associated with the given
  // |render_process_id| and |main_render_frame_id| and begin observing it.
  void StartObservingWebContents(int render_process_id,
                                 int main_render_frame_id);

  // WebContentsObserver overrides: These events indicate that the view of the
  // main frame may have changed.
  void RenderFrameCreated(RenderFrameHost* render_frame_host) final;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) final;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) final;

  // WebContentsObserver override to notify the client that the source size has
  // changed.
  void MainFrameWasResized(bool width_changed) final;

  // WebContentsObserver override to notify the client that the capture target
  // has been permanently lost.
  void WebContentsDestroyed() final;

  // WebContentsObserver overrides to notify the client that the capture target
  // may have changed due to a separate fullscreen widget shown/destroyed.
  void DidShowFullscreenWidget() final;
  void DidDestroyFullscreenWidget() final;

  // Pointer to the RenderWidgetHostView provided in the last run of
  // |callback_|. This is used to eliminate duplicate callback runs.
  RenderWidgetHostView* last_target_view_;

  // TaskRunner corresponding to the thread that called Start().
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Callback to run when the target RenderWidgetHostView has changed.
  ChangeCallback callback_;

  // Callback to run when the target RenderWidgetHostView has resized.
  base::Closure resize_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsTracker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_TRACKER_H_
