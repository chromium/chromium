// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webrtc_event_logger.h"

#include "base/check_op.h"
#include "content/public/browser/browser_thread.h"

namespace content {

WebRtcEventLogger* g_webrtc_event_logger = nullptr;

WebRtcEventLogger* WebRtcEventLogger::Get() {
  return g_webrtc_event_logger;
}

WebRtcEventLogger::WebRtcEventLogger() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!g_webrtc_event_logger);
  g_webrtc_event_logger = this;
  // Checking that g_webrtc_event_logger was never set before, in a way that
  // would not interfere with unit tests, is overkill.
}

WebRtcEventLogger::~WebRtcEventLogger() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(g_webrtc_event_logger, this);
  g_webrtc_event_logger = nullptr;
}

}  // namespace content
