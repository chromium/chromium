// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_

@class BookmarksFolderChooserViewController;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Delegate for presentation events related to
// BookmarksFolderChooserViewController.
@protocol BookmarksFolderChooserViewControllerPresentationDelegate <NSObject>

// Called when user taps on the "New Folder" row on the top of the screen
// (shown only if `allowsNewFolders` is true).
// `parent` is used as the parent folder inside which a new folder will be
// created.
- (void)showBookmarksFolderEditorWithParentFolderNode:
    (const bookmarks::BookmarkNode*)parentNode;
// Called when a bookmark folder is selected. `folder` is the newly selected
// folder.
- (void)bookmarksFolderChooserViewController:
            (BookmarksFolderChooserViewController*)viewController
                         didFinishWithFolder:
                             (const bookmarks::BookmarkNode*)folder;
// Called when the user is done with the picker, either by tapping the Cancel or
// the Back button.
- (void)bookmarksFolderChooserViewControllerDidCancel:
    (BookmarksFolderChooserViewController*)viewController;
// Called when the view controller disappears either through
// 1. swiping right.
// 2. or pressing the back button when cancel button is not available.
- (void)bookmarksFolderChooserViewControllerDidDismiss:
    (BookmarksFolderChooserViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_PRESENTATION_DELEGATE_H_
