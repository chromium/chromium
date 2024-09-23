// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_MUTATOR_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_MUTATOR_H_

#import <Foundation/Foundation.h>

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// A model mutator class for folder chooser view controller to make changes to
// te model.
@protocol BookmarksFolderChooserMutator <NSObject>

// TODO(crbug.com/40252439): Change parameter signature. View controller should
// not know about BookmarkNode.
- (void)setSelectedFolderNode:(const bookmarks::BookmarkNode*)folderNode;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_MUTATOR_H_
