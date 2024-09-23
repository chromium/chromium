// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_COMMON_CONSTANTS_H_
#define COMPONENTS_WEBAPPS_COMMON_CONSTANTS_H_

#include <stddef.h>

namespace webapps {

// The largest reasonable length we'd assume for a meta tag attribute.
extern const size_t kMaxMetaTagAttributeLength;

// Pref key that refers to list of all apps that have been migrated to web apps.
// TODO(crbug.com/40802205):
// Remove this after preinstalled apps are migrated.
extern const char kWebAppsMigratedPreinstalledApps[];

// Maximum allowed screenshot ratio between the max dimension and min dimension.
extern const double kMaximumScreenshotRatio;

// Maximum length of description to be displayed on the richer install dialog.
extern const size_t kMaximumDescriptionLength;

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_COMMON_CONSTANTS_H_
