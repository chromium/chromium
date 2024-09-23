// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class BookmarksEditorViewController;

@protocol BookmarksEditorViewControllerDelegate <NSObject>

// Called when the controller should be dismissed.
- (void)bookmarkEditorWantsDismissal:(BookmarksEditorViewController*)controller;

// Called when the user wants to select a folder to move the bookmark into.
- (void)moveBookmark;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_VIEW_CONTROLLER_DELEGATE_H_
