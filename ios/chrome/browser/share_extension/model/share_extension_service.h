// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_SERVICE_H_
#define IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_SERVICE_H_

#import "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/core/reading_list_model_observer.h"

namespace bookmarks {
class BookmarkModel;
}

class ReadingListModel;
@class ShareExtensionItemReceiver;

// AuthenticationService is the Chrome interface to the iOS shared
// authentication library.
class ShareExtensionService : public KeyedService,
                              public bookmarks::BaseBookmarkModelObserver,
                              public ReadingListModelObserver {
 public:
  ShareExtensionService(bookmarks::BookmarkModel* bookmark_model,
                        ReadingListModel* reading_list_model);

  ShareExtensionService(const ShareExtensionService&) = delete;
  ShareExtensionService& operator=(const ShareExtensionService&) = delete;

  ~ShareExtensionService() override;

  void Initialize();

  // KeyedService implementation.
  void Shutdown() override;

  // ReadingListModelObserver implementation.
  void ReadingListModelLoaded(const ReadingListModel* model) override;

  // BookmarkModelObserver implementation.
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelChanged() override;

 private:
  // Invoked when any of the observed model has finished loading. Will
  // initialize the ShareExtensionItemReceiver if they are all loaded.
  void AnyModelLoaded();

  raw_ptr<ReadingListModel> reading_list_model_;
  bool reading_list_model_loaded_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  bool bookmark_model_loaded_;
  __strong ShareExtensionItemReceiver* receiver_;
};

#endif  // IOS_CHROME_BROWSER_SHARE_EXTENSION_MODEL_SHARE_EXTENSION_SERVICE_H_
