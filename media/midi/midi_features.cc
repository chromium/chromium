// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_features.h"

#include "build/build_config.h"

namespace midi {
namespace features {

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kMidiManagerWinrt, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_APPLE)
BASE_FEATURE(kMidiMacUmp, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace features
}  // namespace midi
