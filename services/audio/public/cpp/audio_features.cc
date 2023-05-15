// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/audio_features.h"

namespace features {

// Enables disallowing MIDI permission by default.
BASE_FEATURE(kBlockMidiByDefault,
             "BlockMidiByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
