// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_INFOBAR_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_INFOBAR_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

// Values for the UMA Mobile.Messages.Save.Card.Modal.Event histogram. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class MobileMessagesSaveCardModalEvent {
  // Save Card Infobar cardholder name was edited.
  EditedCardHolderName = 0,
  // Save Card Infobar expiration month was edited.
  EditedExpirationMonth = 1,
  // Save Card Infobar expiration year was edited.
  EditedExpirationYear = 2,
  // Highest enumerator. Recommended by Histogram metrics best practices.
  kMaxValue = EditedExpirationYear,
};

// Used to record metrics related to Save Card Infobar specific events.
@interface SaveCardInfobarMetricsRecorder : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Records histogram for Modal |event|.
+ (void)recordModalEvent:(MobileMessagesSaveCardModalEvent)event;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_SAVE_CARD_INFOBAR_METRICS_RECORDER_H_
