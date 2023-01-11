// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_proxy.h"

#include "base/functional/bind.h"
#include "base/location.h"
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
  DCHECK(!update_callback_);

  update_callback_ =
      base::BindRepeating(&MediaInternalsProxy::UpdateUIOnUIThread, handler);
  MediaInternals::GetInstance()->AddUpdateCallback(update_callback_);
}

void MediaInternalsProxy::Detach() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  MediaInternals::GetInstance()->RemoveUpdateCallback(update_callback_);
  update_callback_.Reset();
}

void MediaInternalsProxy::GetEverything() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  MediaInternals::GetInstance()->SendHistoricalMediaEvents();
  MediaInternals::GetInstance()->SendGeneralAudioInformation();
#if !BUILDFLAG(IS_ANDROID)
  MediaInternals::GetInstance()->SendAudioFocusState();
#endif
  MediaInternals::GetInstance()->GetRegisteredCdms();

  // Ask MediaInternals for its data on IO thread.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaInternalsProxy::GetEverythingOnIOThread, this));
}

void MediaInternalsProxy::GetEverythingOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(xhwang): Investigate whether we can update on UI thread directly.
  MediaInternals::GetInstance()->SendAudioStreamData();
  MediaInternals::GetInstance()->SendVideoCaptureDeviceCapabilities();
}

// static
void MediaInternalsProxy::UpdateUIOnUIThread(
    MediaInternalsMessageHandler* handler,
    const std::u16string& update) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  handler->OnUpdate(update);
}

}  // namespace content
