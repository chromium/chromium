// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/webrtc_diagnostics.h"

#include "content/browser/webrtc/webrtc_diagnostics_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
WebRtcDiagnostics* WebRtcDiagnostics::GetInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return WebRtcDiagnosticsImpl::GetInstance();
}

}  // namespace content
