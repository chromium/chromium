// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "midi" command-line switches.

#ifndef MEDIA_MIDI_MIDI_SWITCHES_H_
#define MEDIA_MIDI_MIDI_SWITCHES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/midi/midi_export.h"

namespace midi {
namespace features {

#if BUILDFLAG(IS_WIN)
MIDI_EXPORT BASE_DECLARE_FEATURE(kMidiManagerWinrt);
#endif

}  // namespace features
}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_SWITCHES_H_
