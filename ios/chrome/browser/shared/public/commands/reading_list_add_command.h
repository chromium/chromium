// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READING_LIST_ADD_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READING_LIST_ADD_COMMAND_H_

#import <Foundation/Foundation.h>

class GURL;
@class URLWithTitle;

@interface ReadingListAddCommand : NSObject

@property(nonatomic, readonly) NSArray<URLWithTitle*>* URLs;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithURLs:(NSArray<URLWithTitle*>*)URL
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_READING_LIST_ADD_COMMAND_H_
