// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_USER_SCRIPTS_AVAILABILITY_H_
#define EXTENSIONS_COMMON_USER_SCRIPTS_AVAILABILITY_H_

#include "extensions/common/features/feature.h"

namespace extensions::user_scripts_availability {

Feature::FeatureDelegatedAvailabilityCheckMap CreateAvailabilityCheckMap();

}  // namespace extensions::user_scripts_availability

#endif  // EXTENSIONS_COMMON_USER_SCRIPTS_AVAILABILITY_H_
