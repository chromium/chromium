// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOOPBACK_GROUP_MEMBER_H_
#define SERVICES_AUDIO_LOOPBACK_GROUP_MEMBER_H_

#include "services/audio/muteable.h"
#include "services/audio/snoopable.h"

namespace audio {

// Interface for accessing signal data and controlling a members of an audio
// group. A group is defined by a common group identifier that all members
// share.
//
// The purpose of the grouping concept is to allow a feature to identify all
// audio flows that come from the same logical unit, such as a browser tab. The
// audio flows can then be duplicated, or other group-wide control exercised on
// all members (such as audio muting).
class LoopbackGroupMember : public Snoopable, public Muteable {};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOOPBACK_GROUP_MEMBER_H_
