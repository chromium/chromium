// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "midi" features.

#ifndef MEDIA_MIDI_MIDI_FEATURES_H_
#define MEDIA_MIDI_MIDI_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/midi/midi_export.h"

namespace midi {
namespace features {

#if BUILDFLAG(IS_WIN)
MIDI_EXPORT BASE_DECLARE_FEATURE(kMidiManagerWinrt);
#endif

#if BUILDFLAG(IS_APPLE)
MIDI_EXPORT BASE_DECLARE_FEATURE(kMidiMacUmp);
#endif

}  // namespace features
}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_FEATURES_H_
