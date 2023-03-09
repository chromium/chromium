// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BOOKMARK_ADD_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BOOKMARK_ADD_COMMAND_H_

#import <Foundation/Foundation.h>

class GURL;
@class URLWithTitle;
namespace web {
class WebState;
}

// An object of this class will contain the data needed to execute any bookmark
// command for one or more pages.
@interface BookmarkAddCommand : NSObject

// Initializes a command object with the page's `URL` and `title`.
// If `presentFolderChooser` is true, the user will be prompted to choose
// a destination for the bookmarks first. If false, the item will be bookmarked
// immediately and the displayed snackbar message will allow editing to change
// the location if desired.
- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
       presentFolderChooser:(BOOL)presentFolderChooser
    NS_DESIGNATED_INITIALIZER;

// Initializes a command object with the `webState`'s URL and title.
// If `presentFolderChooser` is true, the user will be prompted to choose
// a destination for the bookmarks first. If false, the item will be bookmarked
// immediately and the displayed snackbar message will allow editing to change
// the location if desired.
- (instancetype)initWithWebState:(web::WebState*)webState
            presentFolderChooser:(BOOL)presentFolderChooser;

// Initializes a command object with multiple pages `UrlWithTitle`.
// This implies `presentFolderChooser` is `true` and the user will need
// to select a destination folder before the bookmarks are saved.
- (instancetype)initWithURLs:(NSArray<URLWithTitle*>*)URLs
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The URL and title pairs to bookmark.
@property(nonatomic, readonly) NSArray<URLWithTitle*>* URLs;
// Whether or not the user needs to select the destination folder before the
// bookmarks are saved.
@property(nonatomic, readonly) BOOL presentFolderChooser;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BOOKMARK_ADD_COMMAND_H_
