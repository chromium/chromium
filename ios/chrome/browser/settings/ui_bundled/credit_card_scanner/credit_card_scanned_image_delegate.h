// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNED_IMAGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNED_IMAGE_DELEGATE_H_

#import <CoreMedia/CoreMedia.h>

// A delegate notified when there is a new credit card image.
@protocol CreditCardScannedImageDelegate

// Receives an image from the `viewport` and processes it in the implementation.
// `viewport` defines the rectangle on the screen where a credit card should be
// positioned to be scanned.
- (void)processOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                         viewport:(CGRect)viewport;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CREDIT_CARD_SCANNER_CREDIT_CARD_SCANNED_IMAGE_DELEGATE_H_
