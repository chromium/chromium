// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the
// services/audio module.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_AUDIO_FEATURES_H_
#define SERVICES_AUDIO_PUBLIC_CPP_AUDIO_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "services/audio/public/cpp/audio_features_export.h"

namespace features {

// The features should be documented alongside the definition of their values
// in the .cc file.
AUDIO_FEATURES_EXPORT BASE_DECLARE_FEATURE(kBlockMidiByDefault);

}  // namespace features

#endif  // SERVICES_AUDIO_PUBLIC_CPP_AUDIO_FEATURES_H_
