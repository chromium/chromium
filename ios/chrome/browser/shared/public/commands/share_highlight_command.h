// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARE_HIGHLIGHT_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARE_HIGHLIGHT_COMMAND_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

class GURL;

// Payload object for the command to share a deep-link to selected text on a Web
// page.
@interface ShareHighlightCommand : NSObject

// Initializes the object with the page's `URL`, `title` and the currently
// `selectedText`. `sourceView` represents the view owning the
// selected text, and `sourceRect` represents the starting bounds of that
// text.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
               selectedText:(NSString*)selectedText
                 sourceView:(UIView*)sourceView
                 sourceRect:(CGRect)sourceRect NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Page's URL with text fragments.
@property(nonatomic, readonly) GURL URL;

// Page's title.
@property(copy, nonatomic, readonly) NSString* title;

// Selected text on the page.
@property(copy, nonatomic, readonly) NSString* selectedText;

// View owning the selected text.
@property(nonatomic, readonly, weak) UIView* sourceView;

// Coordinates representing the starting bounds of the selected text inside
// `sourceView`.
@property(nonatomic, readonly, assign) CGRect sourceRect;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SHARE_HIGHLIGHT_COMMAND_H_
