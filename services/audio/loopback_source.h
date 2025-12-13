// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOOPBACK_SOURCE_H_
#define SERVICES_AUDIO_LOOPBACK_SOURCE_H_

#include "services/audio/muteable.h"
#include "services/audio/snoopable.h"

namespace audio {

// Interface for accessing signal data and controlling muting of an audio
// output.
class LoopbackSource : public Snoopable, public Muteable {};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOOPBACK_SOURCE_H_
