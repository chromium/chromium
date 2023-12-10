// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"

#import "base/metrics/histogram_functions.h"

void LogPasswordSharingInteraction(PasswordSharingInteraction action) {
  base::UmaHistogramEnumeration("PasswordManager.PasswordSharingIOS.UserAction",
                                action);
}
