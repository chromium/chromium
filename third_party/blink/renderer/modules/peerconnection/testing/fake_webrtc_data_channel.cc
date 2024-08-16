// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/testing/fake_webrtc_data_channel.h"

#include "third_party/webrtc/api/make_ref_counted.h"

namespace blink {

// static
webrtc::scoped_refptr<FakeWebRTCDataChannel> FakeWebRTCDataChannel::Create() {
  return webrtc::make_ref_counted<FakeWebRTCDataChannel>();
}

void FakeWebRTCDataChannel::RegisterObserver(
    webrtc::DataChannelObserver* observer) {
  ++register_observer_call_count_;
}

void FakeWebRTCDataChannel::UnregisterObserver() {
  ++unregister_observer_call_count_;
}

void FakeWebRTCDataChannel::Close() {
  close_called_ = true;
  state_ = DataState::kClosed;
}

}  // namespace blink
