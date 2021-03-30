// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_FEATURES_H_
#define FUCHSIA_ENGINE_FEATURES_H_

#include "base/feature_list.h"

namespace features {

constexpr base::Feature kHandleMemoryPressureInRenderer{
    "HandleMemoryPressureInRenderer", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features

#endif  // FUCHSIA_ENGINE_FEATURES_H_
