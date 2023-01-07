// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "media/midi/midi_switches.h"

namespace midi {
namespace features {

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kMidiManagerWinrt,
             "MidiManagerWinrt",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace features
}  // namespace midi
