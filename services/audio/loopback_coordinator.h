// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOOPBACK_COORDINATOR_H_
#define SERVICES_AUDIO_LOOPBACK_COORDINATOR_H_

#include "services/audio/group_coordinator.h"
#include "services/audio/loopback_group_member.h"

namespace audio {
using LoopbackCoordinator = GroupCoordinator<LoopbackGroupMember>;
}  // namespace audio

#endif  // SERVICES_AUDIO_LOOPBACK_COORDINATOR_H_
