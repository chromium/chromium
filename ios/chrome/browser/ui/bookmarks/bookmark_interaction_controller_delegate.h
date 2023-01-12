// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_DELEGATE_H_

@class BookmarkInteractionController;

// BookmarkInteractionControllerDelegate provides methods for the controller to
// notify its delegate when certain events occur.
@protocol BookmarkInteractionControllerDelegate

// Called when the controller is going to commit the title or URL change.
- (void)bookmarkInteractionControllerWillCommitTitleOrUrlChange:
    (BookmarkInteractionController*)controller;

// Called when the controller is stopped and the receiver can safely free any
// references to `controller`.
- (void)bookmarkInteractionControllerDidStop:
    (BookmarkInteractionController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_DELEGATE_H_
