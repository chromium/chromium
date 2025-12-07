// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_remover_helper.h"

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/reading_list/model/reading_list_download_service.h"
#import "ios/chrome/browser/reading_list/model/reading_list_download_service_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace reading_list {

ReadingListRemoverHelper::ReadingListRemoverHelper(ProfileIOS* profile) {
  reading_list_model_ = ReadingListModelFactory::GetForProfile(profile);
  reading_list_download_service_ =
      ReadingListDownloadServiceFactory::GetForProfile(profile);
}

ReadingListRemoverHelper::~ReadingListRemoverHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!reading_list_model_);
  DCHECK(!reading_list_download_service_);
}

void ReadingListRemoverHelper::ReadingListModelLoaded(
    const ReadingListModel* reading_list_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(reading_list_model_, reading_list_model);
  DCHECK(scoped_observation_.IsObservingSource(reading_list_model_.get()));
  scoped_observation_.Reset();

  bool model_cleared = reading_list_model_->DeleteAllEntries(location_);
  reading_list_download_service_->Clear();

  ReadlingListItemsRemoved(model_cleared);
}

void ReadingListRemoverHelper::ReadingListModelBeingDeleted(
    const ReadingListModel* reading_list_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(reading_list_model_, reading_list_model);
  DCHECK(scoped_observation_.IsObservingSource(reading_list_model_.get()));
  scoped_observation_.Reset();
  ReadlingListItemsRemoved(false);
}

void ReadingListRemoverHelper::RemoveAllUserReadingListItemsIOS(
    const base::Location& location,
    Callback completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  completion_ = std::move(completion);
  location_ = location;

  if (!reading_list_model_) {
    ReadlingListItemsRemoved(false);
    return;
  }

  // ReadingListModel::AddObserver calls ReadingListModelLoaded if model is
  // already loaded, so there is no need to check.
  scoped_observation_.Observe(reading_list_model_.get());
}

void ReadingListRemoverHelper::ReadlingListItemsRemoved(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reading_list_model_ = nullptr;
  reading_list_download_service_ = nullptr;
  if (completion_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_), success));
  }
}

}  // namespace reading_list
