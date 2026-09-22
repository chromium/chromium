// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_OVERLAY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_OVERLAY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_consumer.h"

@class WatermarkView;

// The watermark overlay view controller.
@interface WatermarkOverlayViewController : UIViewController <WatermarkConsumer>

// The watermark view to be displayed.
@property(nonatomic, readonly) WatermarkView* watermarkView;

@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_UI_WATERMARK_OVERLAY_VIEW_CONTROLLER_H_
