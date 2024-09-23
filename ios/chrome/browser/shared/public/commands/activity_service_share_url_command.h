// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTIVITY_SERVICE_SHARE_URL_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTIVITY_SERVICE_SHARE_URL_COMMAND_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

class GURL;

// Payload object for the command to trigger the URL sharing flow.
@interface ActivityServiceShareURLCommand : NSObject

// Initializes the object with the given `URL`, `title`
// and the `location` of the longpressed URL.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
                 sourceView:(UIView*)sourceView
                 sourceRect:(CGRect)sourceRect NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Page's URL with text fragments.
@property(nonatomic, readonly) GURL URL;

// Page's title.
@property(nonatomic, readonly) NSString* title;

// View owning the selected text.
@property(nonatomic, readonly, weak) UIView* sourceView;

// Coordinates representing the starting bounds of the longpressed text inside
// `sourceView`.
@property(nonatomic, readonly, assign) CGRect sourceRect;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_ACTIVITY_SERVICE_SHARE_URL_COMMAND_H_
