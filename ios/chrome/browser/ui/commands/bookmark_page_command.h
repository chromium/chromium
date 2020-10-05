// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BOOKMARK_PAGE_COMMAND_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BOOKMARK_PAGE_COMMAND_H_

#import <Foundation/Foundation.h>

class GURL;

// Command to handle a bookmark request for a given page. The page will be
// bookmarked unless it already is. In that case, the "edit bookmark" flow
// will be triggered.
@interface BookmarkPageCommand : NSObject

// Initializes a command object with the page's |URL| and |title|.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) const GURL& URL;
@property(copy, nonatomic, readonly) NSString* title;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BOOKMARK_PAGE_COMMAND_H_
