// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/model/features.h"

namespace session::features {

BASE_FEATURE(kSessionRestorationSessionIDCheck,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSessionRestorationFullConversion,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace session::features
