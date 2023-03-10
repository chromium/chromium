// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_mediator.h"

#import "base/containers/contains.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_consumer.h"
#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_mutator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@interface BookmarksFolderChooserMediator () <BookmarksFolderChooserMutator> {
  BookmarkModel* _bookmarkModel;
  std::set<const BookmarkNode*>* _editedNodes;
}

@end

@implementation BookmarksFolderChooserMediator

- (instancetype)initWithBookmarkModel:(BookmarkModel*)model
                          editedNodes:(std::set<const BookmarkNode*>*)nodes {
  DCHECK(model);
  DCHECK(model->loaded());

  self = [super init];
  if (self) {
    _bookmarkModel = model;
    _editedNodes = nodes;
  }
  return self;
}

#pragma mark - BookmarksFolderChooserDataSource

- (const BookmarkNode*)rootFolder {
  return _bookmarkModel->root_node();
}

- (const std::set<const BookmarkNode*>&)editedNodes {
  return *_editedNodes;
}

- (std::vector<const BookmarkNode*>)visibleFolders {
  return bookmark_utils_ios::VisibleNonDescendantNodes(*_editedNodes,
                                                       _bookmarkModel);
}

#pragma mark - BookmarksFolderChooserMutator

- (void)setSelectedFolder:(const BookmarkNode*)folder {
  _selectedFolder = folder;
  [_consumer notifyModelUpdated];
}

- (void)removeFromEditedNodes:(const BookmarkNode*)node {
  _editedNodes->erase(node);
}

@end
