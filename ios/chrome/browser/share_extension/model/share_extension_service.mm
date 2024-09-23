// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_extension/model/share_extension_service.h"

#import "components/bookmarks/browser/bookmark_model.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/share_extension/model/share_extension_item_receiver.h"

ShareExtensionService::ShareExtensionService(
    bookmarks::BookmarkModel* bookmark_model,
    ReadingListModel* reading_list_model)
    : reading_list_model_(reading_list_model),
      reading_list_model_loaded_(false),
      bookmark_model_(bookmark_model),
      bookmark_model_loaded_(false),
      receiver_(nil) {
  DCHECK(bookmark_model);
  DCHECK(reading_list_model);
}

ShareExtensionService::~ShareExtensionService() {}

void ShareExtensionService::Initialize() {
  bookmark_model_->AddObserver(this);
  if (bookmark_model_->loaded()) {
    bookmark_model_loaded_ = true;
    this->AnyModelLoaded();
  }
  reading_list_model_->AddObserver(this);
}

void ShareExtensionService::Shutdown() {
  reading_list_model_->RemoveObserver(this);
  reading_list_model_loaded_ = false;
  bookmark_model_->RemoveObserver(this);
  bookmark_model_loaded_ = false;
  [receiver_ shutdown];
  receiver_ = nil;
}

void ShareExtensionService::ReadingListModelLoaded(
    const ReadingListModel* model) {
  reading_list_model_loaded_ = true;
  this->AnyModelLoaded();
}

void ShareExtensionService::BookmarkModelLoaded(bool ids_reassigned) {
  bookmark_model_loaded_ = true;
  this->AnyModelLoaded();
}

void ShareExtensionService::BookmarkModelChanged() {}

void ShareExtensionService::AnyModelLoaded() {
  if (reading_list_model_loaded_ && bookmark_model_loaded_) {
    DCHECK(!receiver_);
    receiver_ = [[ShareExtensionItemReceiver alloc]
        initWithBookmarkModel:bookmark_model_
             readingListModel:reading_list_model_];
  }
}
