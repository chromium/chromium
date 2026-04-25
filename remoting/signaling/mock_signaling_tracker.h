// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MOCK_SIGNALING_TRACKER_H_
#define REMOTING_SIGNALING_MOCK_SIGNALING_TRACKER_H_

#include "base/time/time.h"
#include "remoting/signaling/signaling_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockSignalingTracker : public SignalingTracker {
 public:
  MockSignalingTracker();
  ~MockSignalingTracker() override;

  MOCK_METHOD(void, OnChannelActive, ());
  MOCK_METHOD(bool, IsChannelActive, (), (const));
  MOCK_METHOD(base::TimeDelta,
              GetLastReportedChannelActiveDuration,
              (),
              (const));
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MOCK_SIGNALING_TRACKER_H_
