// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_mediator.h"
#import "base/check.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BookmarksEditorMediator ()

// Flag to ignore bookmark model changes notifications.
@property(nonatomic, assign) BOOL ignoresBookmarkModelChanges;

@end

@implementation BookmarksEditorMediator

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                             bookmark:(const bookmarks::BookmarkNode*)bookmark {
  self = [super init];
  if (self) {
    DCHECK(bookmarkModel);
    DCHECK(bookmark);
    DCHECK(bookmark->is_url());
    DCHECK(bookmarkModel->loaded());
    _bookmarkModel = bookmarkModel;
    _bookmark = bookmark;
    _folder = bookmark->parent();
  }
  return self;
}

- (void)disconnect {
  self.bookmarkModel = nullptr;
  self.bookmark = nullptr;
  self.folder = nullptr;
}

#pragma mark - BookmarksEditorMutator

- (BOOL*)ignoresBookmarkModelChangesPointer {
  return &_ignoresBookmarkModelChanges;
}

@end
