// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/media/media_internals_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

MediaInternalsProxy::MediaInternalsProxy() {
}

MediaInternalsProxy::~MediaInternalsProxy() {}

void MediaInternalsProxy::Attach(MediaInternalsMessageHandler* handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  handler_ = handler;
  update_callback_ = base::Bind(&MediaInternalsProxy::UpdateUIOnUIThread, this);
  MediaInternals::GetInstance()->AddUpdateCallback(update_callback_);
}

void MediaInternalsProxy::Detach() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  handler_ = nullptr;
  MediaInternals::GetInstance()->RemoveUpdateCallback(update_callback_);
}

void MediaInternalsProxy::GetEverything() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  MediaInternals::GetInstance()->SendHistoricalMediaEvents();
  MediaInternals::GetInstance()->SendGeneralAudioInformation();
#if !defined(OS_ANDROID)
  MediaInternals::GetInstance()->SendAudioFocusState();
#endif

  // Ask MediaInternals for its data on IO thread.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&MediaInternalsProxy::GetEverythingOnIOThread, this));
}

void MediaInternalsProxy::GetEverythingOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(xhwang): Investigate whether we can update on UI thread directly.
  MediaInternals::GetInstance()->SendAudioStreamData();
  MediaInternals::GetInstance()->SendVideoCaptureDeviceCapabilities();
}

void MediaInternalsProxy::UpdateUIOnUIThread(const base::string16& update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Don't forward updates to a destructed UI.
  if (handler_)
    handler_->OnUpdate(update);
}

}  // namespace content
