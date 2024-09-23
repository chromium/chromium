// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/post_default_abandonment/metrics.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"

namespace post_default_abandonment {

void RecordPostDefaultAbandonmentPromoUserAction(UserActionType action) {
  base::UmaHistogramEnumeration("IOS.PostDefaultAbandonmentPromo.UserAction",
                                action);
}

}  // namespace post_default_abandonment
