// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SHARE_HIGHLIGHT_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SHARE_HIGHLIGHT_COMMAND_H_

#import <Foundation/Foundation.h>

class GURL;

// Payload object for the command to share a deep-link to selected text on a Web
// page.
@interface ShareHighlightCommand : NSObject

// Initializes the object with the page's |URL|, |title| and the currently
// |selectedText|.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
               selectedText:(NSString*)selectedText NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Page's URL with text fragments.
@property(nonatomic, readonly) const GURL& URL;

// Page's title.
@property(copy, nonatomic, readonly) NSString* title;

// Selected text on the page.
@property(copy, nonatomic, readonly) NSString* selectedText;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SHARE_HIGHLIGHT_COMMAND_H_
