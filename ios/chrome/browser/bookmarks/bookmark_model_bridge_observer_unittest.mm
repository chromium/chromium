// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/bookmark_model_bridge_observer.h"

#import <memory>

#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "ios/chrome/browser/bookmarks/bookmark_ios_unit_test_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@class TestBookmarkModelBridgeObserver;

@interface TestOwner : NSObject {
 @public
  std::unique_ptr<BookmarkModelBridge> bridge;
}

@property(nonatomic, strong) TestBookmarkModelBridgeObserver* observer;

- (void)bookmarkNodeDeleted;
@end

@implementation TestOwner
- (void)bookmarkNodeDeleted {
  bridge.reset();
  self.observer = nil;
}
@end

@interface TestBookmarkModelBridgeObserver
    : NSObject <BookmarkModelBridgeObserver> {
  id owner;
}

- (void)setOwner:(id)newOwner;
@end

@implementation TestBookmarkModelBridgeObserver

- (void)setOwner:(id)newOwner {
  owner = newOwner;
}

#pragma mark - BookmarkModelBridgeObserver

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
    didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
}

- (void)bookmarkModelRemovedAllNodes:(bookmarks::BookmarkModel*)model {
}

- (void)bookmarkModelLoaded:(bookmarks::BookmarkModel*)model {
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
          didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
           fromParent:(const bookmarks::BookmarkNode*)oldParent
             toParent:(const bookmarks::BookmarkNode*)newParent {
}

- (void)bookmarkModel:(bookmarks::BookmarkModel*)model
        didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  [owner bookmarkNodeDeleted];
}

@end

namespace bookmarks {

namespace {

using BookmarkModelBridgeObserverTest = BookmarkIOSUnitTestSupport;

TEST_F(BookmarkModelBridgeObserverTest,
       NotifyBookmarkNodeChildrenChangedDespiteSelfDestruction) {
  @autoreleasepool {
    const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
    const BookmarkNode* folder = AddFolder(mobile_node, @"title");

    TestOwner* owner = [[TestOwner alloc] init];
    owner.observer = [[TestBookmarkModelBridgeObserver alloc] init];
    [owner.observer setOwner:owner];

    owner->bridge =
        std::make_unique<BookmarkModelBridge>(owner.observer, bookmark_model_);

    // Deleting the folder should not cause a crash.
    bookmark_model_->Remove(folder,
                            bookmarks::metrics::BookmarkEditSource::kOther);
  }
}

}  // namespace

}  // namespace bookmarks
