// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/common/constants.h"

namespace webapps {

const size_t kMaxMetaTagAttributeLength = 2000;

const char kWebAppsMigratedPreinstalledApps[] =
    "web_apps.migrated_default_apps";

// Maximum dimension can't be more than 2.3 times as long as the minimum
// dimension for screenshots.
const double kMaximumScreenshotRatio = 2.3;

const size_t kMaximumDescriptionLength = 300;

}  // namespace webapps
