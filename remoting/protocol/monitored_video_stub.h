// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MONITORED_VIDEO_STUB_H_
#define REMOTING_PROTOCOL_MONITORED_VIDEO_STUB_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "remoting/protocol/video_stub.h"

namespace base {
class ThreadChecker;
} // namespace base

namespace remoting {
namespace protocol {

// MonitoredVideoStub is responsible for notifying the event handler if no
// frames have been received within |connectivity_check_delay|.
// The implementation uses the decorator pattern in which the MonitoredVideoStub
// implements the same interface as the VideoStub. It overrides the
// ProcessVideoPacket function to provide notification to the client when the
// video channel is connected and forward the packet to the underlying
// VideoStub. Multiple decorators can be stacked on top of each other if more
// functionality is needed in the future.
class MonitoredVideoStub : public VideoStub {
 public:
  // Callback to be called when channel state changes.  The Callback should not
  // destroy the MonitoredVideoStub object.
  typedef base::RepeatingCallback<void(bool connected)> ChannelStateCallback;

  static const int kConnectivityCheckDelaySeconds = 2;

  MonitoredVideoStub(
      VideoStub* video_stub,
      base::TimeDelta connectivity_check_delay,
      const ChannelStateCallback& callback);
  ~MonitoredVideoStub() override;

  // VideoStub implementation.
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> packet,
                          base::OnceClosure done) override;

 private:
  void OnConnectivityCheckTimeout();
  void NotifyChannelState(bool connected);

  VideoStub* video_stub_;
  ChannelStateCallback callback_;
  base::ThreadChecker thread_checker_;
  bool is_connected_;
  base::DelayTimer connectivity_check_timer_;

  DISALLOW_COPY_AND_ASSIGN(MonitoredVideoStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_MONITORED_VIDEO_STUB_H_
