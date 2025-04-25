// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/reader_mode/ui/reader_mode_consumer.h"

// View controller for displaying the Reader mode content.
@interface ReaderModeViewController : UIViewController <ReaderModeConsumer>

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_VIEW_CONTROLLER_H_
