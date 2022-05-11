// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/upgrade/utils/features.h"

const base::Feature kUpgradeCenterRefactor{"UpgradeCenterRefactor",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

bool IsUpgradeCenterRefactorEnabled() {
  return base::FeatureList::IsEnabled(kUpgradeCenterRefactor);
}
