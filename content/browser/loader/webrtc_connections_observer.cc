// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/webrtc_connections_observer.h"

#include "content/browser/webrtc/webrtc_internals.h"

namespace content {

WebRtcConnectionsObserver::WebRtcConnectionsObserver(
    const ConnectionsCountChangedCallback& connections_count_changed_callback)
    : connections_count_changed_callback_(connections_count_changed_callback) {
  DCHECK(!connections_count_changed_callback_.is_null());

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (!webrtc_internals)
    return;
  webrtc_internals->AddConnectionsObserver(this);
}

WebRtcConnectionsObserver::~WebRtcConnectionsObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (!webrtc_internals)
    return;
  webrtc_internals->RemoveConnectionsObserver(this);
}

void WebRtcConnectionsObserver::OnConnectionsCountChange(uint32_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connections_count_changed_callback_.Run(count);
}

}  // namespace content
