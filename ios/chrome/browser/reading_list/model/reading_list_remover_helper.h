// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_REMOVER_HELPER_H_
#define IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_REMOVER_HELPER_H_

#import "base/functional/callback.h"
#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/core/reading_list_model_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class ReadingListDownloadService;

namespace reading_list {

// Helper class to asynchronously remove reading list entries.
class ReadingListRemoverHelper : public ReadingListModelObserver {
 public:
  using Callback = base::OnceCallback<void(bool)>;

  explicit ReadingListRemoverHelper(ProfileIOS* profile);

  ReadingListRemoverHelper(const ReadingListRemoverHelper&) = delete;
  ReadingListRemoverHelper& operator=(const ReadingListRemoverHelper&) = delete;

  ~ReadingListRemoverHelper() override;

  // Removes all Reading list items and asynchronously invoke `completion` with
  // boolean indicating success or failure.
  void RemoveAllUserReadingListItemsIOS(const base::Location& location,
                                        Callback completion);

  // ReadingListModelObserver implementation.
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;

 private:
  // Invoked when the reading list items have been deleted. Invoke the
  // completion callback with `success` (invocation is asynchronous so
  // the object won't be deleted immediately).
  void ReadlingListItemsRemoved(bool success);

  Callback completion_;
  base::Location location_;
  raw_ptr<ReadingListModel> reading_list_model_ = nullptr;
  raw_ptr<ReadingListDownloadService> reading_list_download_service_ = nullptr;
  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      scoped_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_MODEL_READING_LIST_REMOVER_HELPER_H_
