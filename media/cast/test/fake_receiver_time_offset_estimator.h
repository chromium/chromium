// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_FAKE_RECEIVER_TIME_OFFSET_ESTIMATOR_H_
#define MEDIA_CAST_TEST_FAKE_RECEIVER_TIME_OFFSET_ESTIMATOR_H_

#include "base/time/time.h"
#include "base/threading/thread_checker.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/receiver_time_offset_estimator.h"

namespace media {
namespace cast {
namespace test {

// This class is used for testing. It will always return the |offset| value
// provided during construction as offset bounds.
class FakeReceiverTimeOffsetEstimator final
    : public ReceiverTimeOffsetEstimator {
 public:
  FakeReceiverTimeOffsetEstimator(base::TimeDelta offset);

  FakeReceiverTimeOffsetEstimator(const FakeReceiverTimeOffsetEstimator&) =
      delete;
  FakeReceiverTimeOffsetEstimator& operator=(
      const FakeReceiverTimeOffsetEstimator&) = delete;

  ~FakeReceiverTimeOffsetEstimator() final;

  // RawReventSubscriber implementations.
  void OnReceiveFrameEvent(const FrameEvent& frame_event) final;
  void OnReceivePacketEvent(const PacketEvent& packet_event) final;

  // ReceiverTimeOffsetEstimator
  bool GetReceiverOffsetBounds(base::TimeDelta* lower_bound,
                               base::TimeDelta* upper_bound) final;

 private:
  const base::TimeDelta offset_;
};

}  // namespace test
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_FAKE_RECEIVER_TIME_OFFSET_ESTIMATOR_H_
