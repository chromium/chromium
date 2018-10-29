// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_shared_state.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/bookmarks/cells/bookmark_table_cell_title_editing.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Minimal acceptable favicon size, in points.
const CGFloat kMinFaviconSizePt = 16.0;

// Desired favicon size, in points.
const CGFloat kDesiredFaviconSizePt = 32.0;

// Minimium spacing between keyboard and the titleText when creating new folder,
// in points.
const CGFloat kKeyboardSpacingPt = 16.0;

// Max number of favicon download requests in the lifespan of this tableView.
const NSUInteger kMaxDownloadFaviconCount = 50;

}  // namespace

@implementation BookmarkHomeSharedState {
  std::set<const bookmarks::BookmarkNode*> _editNodes;
}

@synthesize addingNewFolder = _addingNewFolder;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize currentlyInEditMode = _currentlyInEditMode;
@synthesize currentlyShowingSearchResults = _currentlyShowingSearchResults;
@synthesize editingFolderCell = _editingFolderCell;
@synthesize editingFolderNode = _editingFolderNode;
@synthesize faviconDownloadCount = _faviconDownloadCount;
@synthesize observer = _observer;
@synthesize promoVisible = _promoVisible;
@synthesize tableView = _tableView;
@synthesize tableViewDisplayedRootNode = _tableViewDisplayedRootNode;
@synthesize tableViewModel = _tableViewModel;

- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                    displayedRootNode:
                        (const bookmarks::BookmarkNode*)displayedRootNode {
  if ((self = [super init])) {
    _bookmarkModel = bookmarkModel;
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
  return kDesiredFaviconSizePt;
}

+ (CGFloat)keyboardSpacingPt {
  return kKeyboardSpacingPt;
}

+ (NSUInteger)maxDownloadFaviconCount {
  return kMaxDownloadFaviconCount;
}

@end
