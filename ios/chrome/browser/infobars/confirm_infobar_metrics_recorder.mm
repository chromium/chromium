// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/confirm_infobar_metrics_recorder.h"

#include "base/metrics/histogram_macros.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kInfobarTypeRestoreEventHistogram[] =
    "Mobile.Messages.Confirm.Event.ConfirmInfobarTypeRestore";

const char kInfobarTypeBlockPopupsEventHistogram[] =
    "Mobile.Messages.Confirm.Event.ConfirmInfobarTypeBlockPopups";

@implementation ConfirmInfobarMetricsRecorder

+ (void)recordConfirmInfobarEvent:(MobileMessagesConfirmInfobarEvents)event
            forInfobarConfirmType:(InfobarConfirmType)infobarConfirmType {
  switch (infobarConfirmType) {
    case InfobarConfirmType::kInfobarConfirmTypeRestore:
      UMA_HISTOGRAM_ENUMERATION(kInfobarTypeRestoreEventHistogram, event);
      break;
    case InfobarConfirmType::kInfobarConfirmTypeBlockPopups:
      UMA_HISTOGRAM_ENUMERATION(kInfobarTypeBlockPopupsEventHistogram, event);
      break;
  }
}

@end
