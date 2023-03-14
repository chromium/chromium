// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/home/bookmarks_home_shared_state.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell_title_editing.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Minimium spacing between keyboard and the titleText when creating new folder,
// in points.
const CGFloat kKeyboardSpacingPt = 16.0;

// Max number of favicon download requests in the lifespan of this tableView.
const NSUInteger kMaxDownloadFaviconCount = 50;

}  // namespace

@implementation BookmarksHomeSharedState {
  std::set<const bookmarks::BookmarkNode*> _editNodes;
}

- (instancetype)
    initWithProfileBookmarkModel:(bookmarks::BookmarkModel*)profileBookmarkModel
               displayedRootNode:
                   (const bookmarks::BookmarkNode*)displayedRootNode {
  if ((self = [super init])) {
    _profileBookmarkModel = profileBookmarkModel;
    _tableViewDisplayedRootNode = displayedRootNode;
  }
  return self;
}

- (void)setCurrentlyInEditMode:(BOOL)currentlyInEditMode {
  DCHECK(self.tableView);

  // If not in editing mode but the tableView's editing is ON, it means the
  // table is waiting for a swipe-to-delete confirmation.  In this case, we need
  // to close the confirmation by setting tableView.editing to NO.
  if (!_currentlyInEditMode && self.tableView.editing) {
    self.tableView.editing = NO;
  }
  [self.editingFolderCell stopEdit];
  _currentlyInEditMode = currentlyInEditMode;
  _editNodes.clear();
  [self.observer sharedStateDidClearEditNodes:self];
  [self.tableView setEditing:currentlyInEditMode animated:YES];
}

- (std::set<const bookmarks::BookmarkNode*>&)editNodes {
  return _editNodes;
}

+ (CGFloat)minFaviconSizePt {
  return kMinFaviconSizePt;
}

+ (CGFloat)desiredFaviconSizePt {
  return kDesiredMediumFaviconSizePt;
}

+ (CGFloat)keyboardSpacingPt {
  return kKeyboardSpacingPt;
}

+ (NSUInteger)maxDownloadFaviconCount {
  return kMaxDownloadFaviconCount;
}

@end
