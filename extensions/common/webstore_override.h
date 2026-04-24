// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_WEBSTORE_OVERRIDE_H_
#define EXTENSIONS_COMMON_WEBSTORE_OVERRIDE_H_

#include "extensions/common/features/feature.h"

namespace extensions::webstore_override {

Feature::FeatureDelegatedAvailabilityCheckMap CreateAvailabilityCheckMap();

}  // namespace extensions::webstore_override

#endif  // EXTENSIONS_COMMON_WEBSTORE_OVERRIDE_H_
