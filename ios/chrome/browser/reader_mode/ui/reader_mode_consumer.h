// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_CONSUMER_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_view.h"

@protocol CRWWebViewProxy;

// Consumer for the Reader mode UI.
@protocol ReaderModeConsumer <NSObject>

// Removes the content view.
- (void)removeContentView;

// Sets the Reader mode content view. `contentView` cannot be nil.
- (void)setContentView:(UIView*)contentView
          webViewProxy:(id<CRWWebViewProxy>)webViewProxy
       overscrollStyle:(OverscrollStyle)overscrollStyle;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_CONSUMER_H_
