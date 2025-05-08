// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_CONSUMER_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_CONSUMER_H_

#import <UIKit/UIKit.h>

// Consumer for the Reader mode UI.
@protocol ReaderModeConsumer <NSObject>

// Sets the Reader mode content view.
- (void)setContentView:(UIView*)contentView;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_CONSUMER_H_
