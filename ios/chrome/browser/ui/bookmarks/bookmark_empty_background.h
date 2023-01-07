// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EMPTY_BACKGROUND_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EMPTY_BACKGROUND_H_

#import <UIKit/UIKit.h>

// View shown as a background view of the bookmark table view when there are no
// bookmarks in the table view.
@interface BookmarkEmptyBackground : UIView

// The text displayed in the center of the view, below a large star icon.
@property(nonatomic, copy) NSString* text;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EMPTY_BACKGROUND_H_
