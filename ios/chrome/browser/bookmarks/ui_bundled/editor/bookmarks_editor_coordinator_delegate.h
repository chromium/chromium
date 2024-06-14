// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_DELEGATE_H_

#import <UIKit/UIKit.h>

@class BookmarksEditorCoordinator;

// Delegate for BookmarksEditorCoordinator.
@protocol BookmarksEditorCoordinatorDelegate <NSObject>

// Called when the coordinator editor is done editing.
- (void)bookmarksEditorCoordinatorShouldStop:
    (BookmarksEditorCoordinator*)coordinator;

// Called when the editor is going to commit the title or URL change.
- (void)bookmarkEditorWillCommitTitleOrURLChange:
    (BookmarksEditorCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_DELEGATE_H_
