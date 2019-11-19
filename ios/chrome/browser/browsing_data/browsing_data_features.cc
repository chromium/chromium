// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browsing_data/browsing_data_features.h"

const base::Feature kNewClearBrowsingDataUI{"NewClearBrowsingDataUI",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kWebClearBrowsingData{"WebClearBrowsingData",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

bool IsNewClearBrowsingDataUIEnabled() {
  return base::FeatureList::IsEnabled(kNewClearBrowsingDataUI);
}
