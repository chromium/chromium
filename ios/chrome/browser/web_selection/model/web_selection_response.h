// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_RESPONSE_H_
#define IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_RESPONSE_H_

#import <UIKit/UIKit.h>

#import "base/values.h"

namespace web {
class WebState;
}

// Response object for calls to get the selection of a web page.
@interface WebSelectionResponse : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Parses a serialized response stored in `dict` into a WebSelectionResponse.
// `webState` must not be null.
+ (instancetype)selectionResponseWithDict:(const base::Value::Dict&)dict
                                 webState:(web::WebState*)webState;

// Return a response with every field nil and `valid`=NO.
+ (instancetype)invalidResponse;

// Whether the other values result from an actual JS response.
// If valid is NO, an error occurred when retrieving the selection
// and the other values of the response will be nil or zero.
@property(nonatomic, readonly, assign, getter=isValid) BOOL valid;

// The selected text.
@property(nonatomic, readonly, copy) NSString* selectedText;

// The view owning the selected text.
@property(nonatomic, readonly, weak) UIView* sourceView;

// Coordinates showing where the selected text is located inside the owning
// view.
// Note: if `selectedText` is empty, `sourceRect` can be CGRectZero if there was
// no selection or non zero if the selection contained no text (it could contain
// an image).
// Note: sourceRect is in page coordinate and does not take into account the
// view inset.
@property(nonatomic, readonly, assign) CGRect sourceRect;

@end

#endif  // IOS_CHROME_BROWSER_WEB_SELECTION_MODEL_WEB_SELECTION_RESPONSE_H_
