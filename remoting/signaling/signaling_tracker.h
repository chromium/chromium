// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SIGNALING_TRACKER_H_
#define REMOTING_SIGNALING_SIGNALING_TRACKER_H_

namespace remoting {

// Interface to track signaling related information. Useful for telemetry and
// debugging.
class SignalingTracker {
 public:
  virtual ~SignalingTracker() = default;

  // Called whenever a signaling activity is detected. For an active signaling
  // channel, this should be called not less than once per minute.
  virtual void OnSignalingActive() = 0;

 protected:
  SignalingTracker() = default;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SIGNALING_TRACKER_H_
