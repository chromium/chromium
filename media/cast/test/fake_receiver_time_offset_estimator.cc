// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/fake_receiver_time_offset_estimator.h"

namespace media {
namespace cast {
namespace test {

FakeReceiverTimeOffsetEstimator::FakeReceiverTimeOffsetEstimator(
    base::TimeDelta offset)
    : offset_(offset) {}

FakeReceiverTimeOffsetEstimator::~FakeReceiverTimeOffsetEstimator() = default;

void FakeReceiverTimeOffsetEstimator::OnReceiveFrameEvent(
    const FrameEvent& frame_event) {
  // Do nothing.
}

void FakeReceiverTimeOffsetEstimator::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  // Do nothing.
}

bool FakeReceiverTimeOffsetEstimator::GetReceiverOffsetBounds(
    base::TimeDelta* lower_bound,
    base::TimeDelta* upper_bound) {
  *lower_bound = offset_;
  *upper_bound = offset_;
  return true;
}

}  // namespace test
}  // namespace cast
}  // namespace media
