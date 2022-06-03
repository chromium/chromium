// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_BANDWIDTH_ESTIMATOR_H_
#define REMOTING_PROTOCOL_BANDWIDTH_ESTIMATOR_H_

#include "remoting/codec/webrtc_video_encoder.h"

namespace remoting {
namespace protocol {

// An interface to collect information from various sources and estimate the
// network bandwidth.
class BandwidthEstimator {
 public:
  BandwidthEstimator() = default;
  virtual ~BandwidthEstimator() = default;

  // Called before sending a |frame|.
  virtual void OnSendingFrame(const WebrtcVideoEncoder::EncodedFrame& frame) {}

  // Called after a frame has been ACKED by the other end of the peer. This
  // function may not be called after each OnSendingFrame(), the frame may be
  // dropped by network.
  virtual void OnReceivedAck() {}

  // Called at any time when an external estimator reports an estimate of
  // bitrate.
  virtual void OnBitrateEstimation(int bitrate_kbps) {}

  // Returns the estimate of the bitrate. This function returns 0 if not enough
  // data have been received for the implementation to make a decision.
  virtual int GetBitrateKbps() = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_BANDWIDTH_ESTIMATOR_H_
