// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_CONNECTIONS_OBSERVER_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_CONNECTIONS_OBSERVER_H_

#include "base/observer_list_types.h"

namespace content {

// Implement this interface to receive WebRTCInternals updates.
class WebRtcInternalsConnectionsObserver : public base::CheckedObserver {
 public:
  ~WebRtcInternalsConnectionsObserver() override {}

  // Called when there is a change in the count of active WebRTC connections.
  virtual void OnConnectionsCountChange(uint32_t count) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_INTERNALS_CONNECTIONS_OBSERVER_H_
