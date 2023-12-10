// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/confirm_infobar_metrics_recorder.h"

#import "base/metrics/histogram_macros.h"

const char kInfobarTypeRestoreEventHistogram[] =
    "Mobile.Messages.Confirm.Event.ConfirmInfobarTypeRestore";
const char kInfobarTypeRestoreAcceptTimeHistogram[] =
    "Mobile.Messages.Confirm.Accept.Time.ConfirmInfobarTypeRestore";

const char kInfobarTypeBlockPopupsEventHistogram[] =
    "Mobile.Messages.Confirm.Event.ConfirmInfobarTypeBlockPopups";
const char kInfobarTypeBlockPopupsAcceptTimeHistogram[] =
    "Mobile.Messages.Confirm.Accept.Time.ConfirmInfobarTypeBlockPopups";

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

+ (void)recordConfirmAcceptTime:(NSTimeInterval)duration
          forInfobarConfirmType:(InfobarConfirmType)infobarConfirmType {
  base::TimeDelta timeDelta = base::Seconds(duration);
  switch (infobarConfirmType) {
    case InfobarConfirmType::kInfobarConfirmTypeRestore:
      UMA_HISTOGRAM_MEDIUM_TIMES(kInfobarTypeRestoreAcceptTimeHistogram,
                                 timeDelta);
      break;
    case InfobarConfirmType::kInfobarConfirmTypeBlockPopups:
      UMA_HISTOGRAM_MEDIUM_TIMES(kInfobarTypeBlockPopupsAcceptTimeHistogram,
                                 timeDelta);
      break;
  }
}

@end
