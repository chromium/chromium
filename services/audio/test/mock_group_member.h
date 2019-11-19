// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_MOCK_GROUP_MEMBER_H_
#define SERVICES_AUDIO_TEST_MOCK_GROUP_MEMBER_H_

#include "services/audio/group_coordinator.h"

#include "base/macros.h"
#include "media/base/audio_parameters.h"
#include "services/audio/loopback_group_member.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace audio {

class MockGroupMember : public LoopbackGroupMember {
 public:
  MockGroupMember();
  ~MockGroupMember() override;

  MOCK_CONST_METHOD0(GetAudioParameters, const media::AudioParameters&());
  MOCK_CONST_METHOD0(GetDeviceId, std::string());
  MOCK_METHOD1(StartSnooping, void(Snooper* snooper));
  MOCK_METHOD1(StopSnooping, void(Snooper* snooper));
  MOCK_METHOD0(StartMuting, void());
  MOCK_METHOD0(StopMuting, void());
  MOCK_METHOD0(IsMuting, bool());

  DISALLOW_COPY_AND_ASSIGN(MockGroupMember);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_MOCK_GROUP_MEMBER_H_
