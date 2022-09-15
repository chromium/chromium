// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_TEST_MOCK_GROUP_COORDINATOR_H_
#define SERVICES_AUDIO_TEST_MOCK_GROUP_COORDINATOR_H_

#include "services/audio/group_coordinator.h"
#include "services/audio/test/mock_group_member.h"

namespace audio {
using MockGroupCoordinator = GroupCoordinator<MockGroupMember>;
}  // namespace audio

#endif  // SERVICES_AUDIO_TEST_MOCK_GROUP_COORDINATOR_H_
