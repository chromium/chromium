// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/sync/utils/features.h"

const base::Feature kDisplaySyncErrorsRefactor{
    "DisplaySyncErrorsRefactor", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsDisplaySyncErrorsRefactorEnabled() {
  return base::FeatureList::IsEnabled(kDisplaySyncErrorsRefactor);
}
