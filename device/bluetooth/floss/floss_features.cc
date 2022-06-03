// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_features.h"

namespace floss {
namespace features {

// Enables Floss client if supported by platform
const base::Feature kFlossEnabled{"Floss", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace floss
