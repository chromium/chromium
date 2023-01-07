// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_SEARCH_WIDGET_EXTENSION_COPIED_CONTENT_VIEW_H_
#define IOS_CHROME_SEARCH_WIDGET_EXTENSION_COPIED_CONTENT_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/elements/highlight_button.h"

typedef NS_ENUM(NSInteger, CopiedContentType) {
  CopiedContentTypeNone,
  CopiedContentTypeURL,
  CopiedContentTypeString,
  CopiedContentTypeImage,
};

// View to show and allow opening of the copied URL. Shows a button with the
// `copiedURLString` if it has been set. When tapped, `actionSelector` in
// `target` is called. If no `copiedURLString` was set, the button is replaced
// by a hairline separation and placeholder text.
@interface CopiedContentView : HighlightButton

// Designated initializer, creates the copiedURLView with a `target` to open the
// URL.
- (instancetype)initWithActionTarget:(id)target
                      actionSelector:(SEL)actionSelector
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

- (void)setCopiedContentType:(CopiedContentType)type;

@end

#endif  // IOS_CHROME_SEARCH_WIDGET_EXTENSION_COPIED_CONTENT_VIEW_H_
