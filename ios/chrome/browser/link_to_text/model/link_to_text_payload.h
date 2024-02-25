// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_PAYLOAD_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_PAYLOAD_H_

#import <UIKit/UIKit.h>

#import "url/gurl.h"

// This object holds necessary information for sharing a deep-link to text on a
// Web page.
@interface LinkToTextPayload : NSObject

// Initializes an object with the `URL` of the Web page containing text
// fragments, the page's `title`, the `selectedText` itself, the `sourceView`
// which contains the text, and `sourceRect` showing where that text is located.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
               selectedText:(NSString*)selectedText
                 sourceView:(UIView*)sourceView
                 sourceRect:(CGRect)sourceRect NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// URL of the page containing the relevant fragments allowing to deep-link to
// the selected text.
@property(nonatomic, readonly, assign) GURL URL;

// The Web page's title.
@property(nonatomic, readonly, copy) NSString* title;

// The selected text to the shared.
@property(nonatomic, readonly, copy) NSString* selectedText;

// The view owning the selected text.
@property(nonatomic, readonly, weak) UIView* sourceView;

// Coordinates showing where the selected text is located inside the owning
// view.
@property(nonatomic, readonly, assign) CGRect sourceRect;

@end

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_PAYLOAD_H_
