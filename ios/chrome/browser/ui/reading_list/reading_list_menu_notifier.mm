// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_menu_notifier.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/core/reading_list_model_observer.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_notification_delegate.h"

class ReadingListObserverBridge;

@interface ReadingListMenuNotifier () {
  // Observer for reading list changes.
  std::unique_ptr<ReadingListObserverBridge> _readingListObserverBridge;

  // Backing object for property of the same name.
  __weak id<ReadingListMenuNotificationDelegate> _delegate;

  // Keep a reference to detach before deallocing.
  raw_ptr<ReadingListModel> _readingListModel;  // weak
}

// Detach the observer on the reading list.
- (void)detachReadingListModel;

// Handle callbacks from the reading list model observer.
- (void)readingListModelCompletedBatchUpdates:(const ReadingListModel*)model;

@end

// TODO(crbug.com/41241675): use the one-and-only protocol-based implementation
// of ReadingListModelObserver
class ReadingListObserverBridge : public ReadingListModelObserver {
 public:
  explicit ReadingListObserverBridge(ReadingListMenuNotifier* owner)
      : owner_(owner) {}

  ~ReadingListObserverBridge() override {}

  void ReadingListModelLoaded(const ReadingListModel* model) override {}

  void ReadingListModelBeganBatchUpdates(
      const ReadingListModel* model) override {}

  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override {
    [owner_ readingListModelCompletedBatchUpdates:model];
  }

  void ReadingListModelBeingDeleted(const ReadingListModel* model) override {}

  void ReadingListDidApplyChanges(ReadingListModel* model) override {
    [owner_ readingListModelCompletedBatchUpdates:model];
  }

 private:
  __weak ReadingListMenuNotifier* owner_;  // weak, owns us
};

@implementation ReadingListMenuNotifier
@synthesize delegate = _delegate;

- (instancetype)initWithReadingList:(ReadingListModel*)readingListModel {
  if ((self = [super init])) {
    _readingListObserverBridge.reset(new ReadingListObserverBridge(self));
    _readingListModel = readingListModel;
    _readingListModel->AddObserver(_readingListObserverBridge.get());
  }
  return self;
}

- (void)dealloc {
  [self detachReadingListModel];
}

- (void)detachReadingListModel {
  _readingListModel->RemoveObserver(_readingListObserverBridge.get());
  _readingListObserverBridge.reset();
}

- (void)readingListModelCompletedBatchUpdates:(const ReadingListModel*)model {
  [_delegate unreadCountChanged:model->unread_size()];
}

- (NSInteger)readingListUnreadCount {
  return _readingListModel->unread_size();
}

@end
