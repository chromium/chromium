// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ALERT_VIEW_UI_BUNDLED_ALERT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_ALERT_VIEW_UI_BUNDLED_ALERT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/alert_view/ui_bundled/alert_consumer.h"

// This class is a replacement for UIAlertController that supports custom
// presentation styles, i.e. change modalPresentationStyle,
// modalTransitionStyle, or transitioningDelegate. The style is more similar to
// the rest of Chromium. Current limitations:
//     Action Sheet Style is not supported.
@interface AlertViewController : UIViewController <AlertConsumer>

// The text in the text fields after presentation.
@property(nonatomic, readonly) NSArray<NSString*>* textFieldResults;

@end

#endif  // IOS_CHROME_BROWSER_ALERT_VIEW_UI_BUNDLED_ALERT_VIEW_CONTROLLER_H_
