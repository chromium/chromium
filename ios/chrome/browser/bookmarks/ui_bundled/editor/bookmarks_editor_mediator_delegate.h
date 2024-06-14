// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_MEDIATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

@class BookmarksEditorMediator;
@class MDCSnackbarMessage;

// Delegate allowing the bookmarks editor mediator to update the coordinator if
// needed.
@protocol BookmarksEditorMediatorDelegate <NSObject>

// Called when the controller should be dismissed.
- (void)bookmarkEditorMediatorWantsDismissal:(BookmarksEditorMediator*)mediator;

// Change the folder in the folder selector.
- (void)bookmarkDidMoveToParent:(const bookmarks::BookmarkNode*)newParent;

// Called when the controller is going to commit the title or URL change.
- (void)bookmarkEditorWillCommitTitleOrURLChange:
    (BookmarksEditorMediator*)mediator;
@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_MEDIATOR_DELEGATE_H_
