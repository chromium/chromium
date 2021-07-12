// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_READING_LIST_ADD_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_READING_LIST_ADD_COMMAND_H_

#import <Foundation/Foundation.h>

class GURL;

@interface ReadingListAddCommand : NSObject

@property(nonatomic, readonly) const GURL& URL;
@property(copy, nonatomic, readonly) NSString* title;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_READING_LIST_ADD_COMMAND_H_
