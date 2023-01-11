// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_MONITORED_VIDEO_STUB_H_
#define REMOTING_PROTOCOL_MONITORED_VIDEO_STUB_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "remoting/protocol/video_stub.h"

namespace remoting::protocol {

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

  MonitoredVideoStub(VideoStub* video_stub,
                     base::TimeDelta connectivity_check_delay,
                     const ChannelStateCallback& callback);

  MonitoredVideoStub(const MonitoredVideoStub&) = delete;
  MonitoredVideoStub& operator=(const MonitoredVideoStub&) = delete;

  ~MonitoredVideoStub() override;

  // VideoStub implementation.
  void ProcessVideoPacket(std::unique_ptr<VideoPacket> packet,
                          base::OnceClosure done) override;

 private:
  void OnConnectivityCheckTimeout();
  void NotifyChannelState(bool connected);

  raw_ptr<VideoStub> video_stub_;
  ChannelStateCallback callback_;
  bool is_connected_;
  base::DelayTimer connectivity_check_timer_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_MONITORED_VIDEO_STUB_H_
