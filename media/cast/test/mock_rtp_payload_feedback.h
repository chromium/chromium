// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_MOCK_RTP_PAYLOAD_FEEDBACK_H_
#define MEDIA_CAST_TEST_MOCK_RTP_PAYLOAD_FEEDBACK_H_

#include "media/cast/net/rtp/rtp_defines.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

class MockRtpPayloadFeedback : public RtpPayloadFeedback {
 public:
  MockRtpPayloadFeedback();
  ~MockRtpPayloadFeedback() override;

  MOCK_METHOD1(CastFeedback, void(const RtcpCastMessage& cast_feedback));
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_MOCK_RTP_PAYLOAD_FEEDBACK_H_
