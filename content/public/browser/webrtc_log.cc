// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webrtc_log.h"

#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
void WebRtcLog::SetLogMessageCallback(
    int render_process_id,
    base::RepeatingCallback<void(const std::string&)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaStreamManager::RegisterNativeLogCallback(render_process_id,
                                                std::move(callback));
}

// static
void WebRtcLog::ClearLogMessageCallback(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaStreamManager::UnregisterNativeLogCallback(render_process_id);
}

}  // namespace content
