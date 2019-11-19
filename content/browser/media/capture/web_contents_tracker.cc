// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/web_contents_tracker.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace content {

WebContentsTracker::WebContentsTracker() : last_target_view_(nullptr) {}

WebContentsTracker::~WebContentsTracker() {
  // Likely unintentional BUG if Stop() was not called before this point.
  DCHECK(!web_contents());
}

void WebContentsTracker::Start(int render_process_id, int main_render_frame_id,
                               const ChangeCallback& callback) {
  DCHECK(!task_runner_ || task_runner_->BelongsToCurrentThread());

  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(task_runner_);
  callback_ = callback;

  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    StartObservingWebContents(render_process_id, main_render_frame_id);
  } else {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&WebContentsTracker::StartObservingWebContents, this,
                       render_process_id, main_render_frame_id));
  }
}

void WebContentsTracker::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  callback_.Reset();
  resize_callback_.Reset();

  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    WebContentsObserver::Observe(nullptr);
  } else {
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&WebContentsTracker::Observe, this,
                                  static_cast<WebContents*>(nullptr)));
  }
}

RenderWidgetHostView* WebContentsTracker::GetTargetView() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* const wc = web_contents();
  if (!wc)
    return nullptr;

  if (auto* view = wc->GetRenderWidgetHostView()) {
    // Make sure the RWHV is still associated with a RWH before considering the
    // view "alive." This is because a null RWH indicates the RWHV has had its
    // Destroy() method called.
    if (view->GetRenderWidgetHost())
      return view;
  }
  return nullptr;
}

void WebContentsTracker::SetResizeChangeCallback(
    const base::Closure& callback) {
  DCHECK(!task_runner_ || task_runner_->BelongsToCurrentThread());
  resize_callback_ = callback;
}

void WebContentsTracker::OnPossibleTargetChange(bool force_callback_run) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderWidgetHostView* const rwhv = GetTargetView();
  if (rwhv == last_target_view_ && !force_callback_run) {
    DVLOG(1) << "No target view change (RenderWidgetHostView@" << rwhv << ')';
    return;
  }
  DVLOG(1) << "Will report target change from RenderWidgetHostView@"
           << last_target_view_ << " to RenderWidgetHostView@" << rwhv;
  last_target_view_ = rwhv;

  if (task_runner_->BelongsToCurrentThread()) {
    MaybeDoCallback(is_still_tracking());
    return;
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebContentsTracker::MaybeDoCallback, this,
                                is_still_tracking()));
}

void WebContentsTracker::MaybeDoCallback(bool was_still_tracking) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Notify of a size change just before notifying of a new target. This allows
  // the downstream implementation to capture the first frame from the new
  // target at the correct resolution. http://crbug.com/704277
  if (was_still_tracking)
    MaybeDoResizeCallback();
  if (!callback_.is_null())
    callback_.Run(was_still_tracking);
}

void WebContentsTracker::MaybeDoResizeCallback() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!resize_callback_.is_null())
    resize_callback_.Run();
}

void WebContentsTracker::StartObservingWebContents(int render_process_id,
                                                   int main_render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Observe(WebContents::FromRenderFrameHost(RenderFrameHost::FromID(
      render_process_id, main_render_frame_id)));
  DVLOG_IF(1, !web_contents())
      << "Could not find WebContents associated with main RenderFrameHost "
      << "referenced by render_process_id=" << render_process_id
      << ", routing_id=" << main_render_frame_id;

  OnPossibleTargetChange(true);
}

void WebContentsTracker::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  DVLOG(1) << "RenderFrameCreated(rfh=" << render_frame_host << ')';
  OnPossibleTargetChange(false);
}

void WebContentsTracker::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  DVLOG(1) << "RenderFrameDeleted(rfh=" << render_frame_host << ')';
  OnPossibleTargetChange(false);
}

void WebContentsTracker::RenderFrameHostChanged(RenderFrameHost* old_host,
                                                RenderFrameHost* new_host) {
  DVLOG(1) << "RenderFrameHostChanged(old=" << old_host << ", new=" << new_host
           << ')';
  OnPossibleTargetChange(false);
}

void WebContentsTracker::MainFrameWasResized(bool width_changed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (task_runner_->BelongsToCurrentThread()) {
    MaybeDoResizeCallback();
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebContentsTracker::MaybeDoResizeCallback, this));
}

void WebContentsTracker::WebContentsDestroyed() {
  DVLOG(1) << "WebContentsDestroyed()";
  Observe(nullptr);
  OnPossibleTargetChange(true);
}

void WebContentsTracker::DidShowFullscreenWidget() {
  DVLOG(1) << "DidShowFullscreenWidget()";
  OnPossibleTargetChange(false);
}

void WebContentsTracker::DidDestroyFullscreenWidget() {
  DVLOG(1) << "DidDestroyFullscreenWidget()";
  OnPossibleTargetChange(false);
}

}  // namespace content
