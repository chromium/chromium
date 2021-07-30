// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BOOKMARK_ADD_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BOOKMARK_ADD_COMMAND_H_

#import <Foundation/Foundation.h>

class GURL;
@class URLWithTitle;

// An object of this class will contain the data needed to execute any bookmark
// command for one or more pages.
@interface BookmarkAddCommand : NSObject

// Initializes a command object with the page's |URL| and |title|.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title NS_DESIGNATED_INITIALIZER;

// Initializes a command object with multiple pages |UrlWithTitle|.
- (instancetype)initWithURLs:(NSArray<URLWithTitle*>*)URLs
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) NSArray<URLWithTitle*>* URLs;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BOOKMARK_ADD_COMMAND_H_
