// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_VIEW_CONTROLLER_DELEGATE_H_

@class BookmarksEditorViewController;

@protocol BookmarksEditorViewControllerDelegate

// Called when the controller should be dismissed.
- (void)bookmarkEditorWantsDismissal:(BookmarksEditorViewController*)controller;

// Called when the controller is going to commit the title or URL change.
- (void)bookmarkEditorWillCommitTitleOrURLChange:
    (BookmarksEditorViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_VIEW_CONTROLLER_DELEGATE_H_
