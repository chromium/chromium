// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"

#import <memory>

#import "components/bookmarks/common/bookmark_metrics.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"

@class TestBookmarkModelBridgeObserver;

@interface TestBookmarkModelBridgeOwner : NSObject

- (instancetype)initWithModel:(bookmarks::BookmarkModel*)model
                     observer:(TestBookmarkModelBridgeObserver*)observer
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)bookmarkNodeDeleted;

- (bool)bookmarkNodeDeletedCalled;

@end

@interface TestBookmarkModelBridgeObserver
    : NSObject <BookmarkModelBridgeObserver> {
  __weak TestBookmarkModelBridgeOwner* _owner;
}

- (void)setOwner:(TestBookmarkModelBridgeOwner*)owner;
@end

@implementation TestBookmarkModelBridgeOwner {
  std::unique_ptr<BookmarkModelBridge> _bridge;
  TestBookmarkModelBridgeObserver* _observer;
  bool _bookmarkNodeDeletedCalled;
}

- (instancetype)initWithModel:(bookmarks::BookmarkModel*)model
                     observer:(TestBookmarkModelBridgeObserver*)observer {
  if ((self = [super init])) {
    DCHECK(model);
    _observer = observer;
    [_observer setOwner:self];

    _bridge = std::make_unique<BookmarkModelBridge>(_observer, model);
  }
  return self;
}

- (void)bookmarkNodeDeleted {
  _bookmarkNodeDeletedCalled = true;
  _bridge.reset();
  _observer = nil;
}

- (bool)bookmarkNodeDeletedCalled {
  return _bookmarkNodeDeletedCalled;
}

@end

@implementation TestBookmarkModelBridgeObserver

- (void)setOwner:(TestBookmarkModelBridgeOwner*)owner {
  _owner = owner;
}

#pragma mark - BookmarkModelBridgeObserver

- (void)didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
}

- (void)bookmarkModelRemovedAllNodes {
}

- (void)bookmarkModelLoaded {
}

- (void)didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
}

- (void)didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
         fromParent:(const bookmarks::BookmarkNode*)oldParent
           toParent:(const bookmarks::BookmarkNode*)newParent {
}

- (void)didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  [_owner bookmarkNodeDeleted];
}

@end

namespace bookmarks {

namespace {

using BookmarkModelBridgeObserverTest = BookmarkIOSUnitTestSupport;

TEST_F(BookmarkModelBridgeObserverTest,
       NotifyBookmarkNodeChildrenChangedDespiteSelfDestruction) {
  @autoreleasepool {
    const BookmarkNode* mobile_node = bookmark_model_->mobile_node();
    const BookmarkNode* folder = AddFolder(mobile_node, u"title");

    TestBookmarkModelBridgeOwner* owner = [[TestBookmarkModelBridgeOwner alloc]
        initWithModel:bookmark_model_
             observer:[[TestBookmarkModelBridgeObserver alloc] init]];

    // Deleting the folder should not cause a crash.
    bookmark_model_->Remove(
        folder, bookmarks::metrics::BookmarkEditSource::kOther, FROM_HERE);

    EXPECT_TRUE([owner bookmarkNodeDeletedCalled]);
  }
}

}  // namespace

}  // namespace bookmarks
