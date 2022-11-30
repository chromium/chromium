// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_coordinator.h"
#include "services/audio/group_coordinator-impl.h"

namespace audio {
template class GroupCoordinator<LoopbackGroupMember>;
}  // namespace audio
