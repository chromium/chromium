// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_CONFIRM_INFOBAR_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_CONFIRM_INFOBAR_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/infobars/model/infobar_type.h"

// Histogram names for InfobarConfirmTypeRestore.
extern const char kInfobarTypeRestoreEventHistogram[];

// Histogram names for ConfirmInfobarTypeBlockPopups.
extern const char kInfobarTypeBlockPopupsEventHistogram[];

// Values for the UMA Mobile.Messages.Confirm.Event histogram. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class MobileMessagesConfirmInfobarEvents {
  // ConfrimInfobar was presented.
  Presented = 0,
  // ConfrimInfobar was accepted.
  Accepted = 1,
  // ConfrimInfobar was dismissed.
  Dismissed = 2,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = Dismissed,
};

// Used to record metrics related to Confirm Infobar events.
@interface ConfirmInfobarMetricsRecorder : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Records histogram `event` for ConfirmInfobar of type `infobarConfirmType`.
+ (void)recordConfirmInfobarEvent:(MobileMessagesConfirmInfobarEvents)event
            forInfobarConfirmType:(InfobarConfirmType)infobarConfirmType;

// Records the `duration` since the Infobar delegate was created until it was
// accepted for ConfirmInfobar of type `infobarConfirmType`.
+ (void)recordConfirmAcceptTime:(NSTimeInterval)duration
          forInfobarConfirmType:(InfobarConfirmType)infobarConfirmType;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_CONFIRM_INFOBAR_METRICS_RECORDER_H_
