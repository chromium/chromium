// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/model/features.h"

namespace session::features {

BASE_FEATURE(kSessionRestorationSessionIDCheck,
             "SessionRestorationSessionIDCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableSessionSerializationOptimizations,
             "EnableSessionSerializationOptimizations",
             base::FEATURE_ENABLED_BY_DEFAULT);

bool UseSessionSerializationOptimizations() {
  return base::FeatureList::IsEnabled(kEnableSessionSerializationOptimizations);
}

}  // namespace session::features
