// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "base/metrics/histogram_functions.h"

namespace password_manager {

const char kPasswordManagerSurfaceVisitHistogramName[] =
    "PasswordManager.iOS.SurfaceVisit";

void LogPasswordManagerSurfaceVisit(PasswordManagerSurface visited_surface) {
  base::UmaHistogramEnumeration(
      /*name=*/kPasswordManagerSurfaceVisitHistogramName,
      /*sample=*/visited_surface);
}

}  // namespace password_manager
