// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_WEBRTC_CONNECTIONS_OBSERVER_H_
#define CONTENT_BROWSER_WEBRTC_WEBRTC_CONNECTIONS_OBSERVER_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "content/browser/webrtc/webrtc_internals_connections_observer.h"

namespace content {

class WebRtcConnectionsObserver : public WebRtcInternalsConnectionsObserver {
 public:
  typedef base::RepeatingCallback<void(uint32_t)>
      ConnectionsCountChangedCallback;
  // |webrtc_connections_count_changed_callback| is called every time there
  // is a change in the count of active WebRTC connections.
  explicit WebRtcConnectionsObserver(const ConnectionsCountChangedCallback&
                                         connections_count_changed_callback);

  WebRtcConnectionsObserver(const WebRtcConnectionsObserver&) = delete;
  WebRtcConnectionsObserver& operator=(const WebRtcConnectionsObserver&) =
      delete;

  ~WebRtcConnectionsObserver() override;

 private:
  // content::WebRtcInternalsConnectionsObserver:
  void OnConnectionsCountChange(uint32_t count) override;

  ConnectionsCountChangedCallback connections_count_changed_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_WEBRTC_CONNECTIONS_OBSERVER_H_
