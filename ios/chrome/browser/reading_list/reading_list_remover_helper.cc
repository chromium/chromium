// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_remover_helper.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"

namespace reading_list {

ReadingListRemoverHelper::ReadingListRemoverHelper(
    ios::ChromeBrowserState* browser_state) {
  reading_list_model_ =
      ReadingListModelFactory::GetForBrowserState(browser_state);
  reading_list_download_service_ =
      ReadingListDownloadServiceFactory::GetForBrowserState(browser_state);
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
  scoped_observer_.Remove(reading_list_model_);

  bool model_cleared = reading_list_model_->DeleteAllEntries();
  reading_list_download_service_->Clear();

  ReadlingListItemsRemoved(model_cleared);
}

void ReadingListRemoverHelper::ReadingListModelBeingDeleted(
    const ReadingListModel* reading_list_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(reading_list_model_, reading_list_model);
  scoped_observer_.Remove(reading_list_model_);
  ReadlingListItemsRemoved(false);
}

void ReadingListRemoverHelper::RemoveAllUserReadingListItemsIOS(
    Callback completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  completion_ = std::move(completion);

  if (!reading_list_model_) {
    ReadlingListItemsRemoved(false);
    return;
  }

  // ReadingListModel::AddObserver calls ReadingListModelLoaded if model is
  // already loaded, so there is no need to check.
  scoped_observer_.Add(reading_list_model_);
}

void ReadingListRemoverHelper::ReadlingListItemsRemoved(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reading_list_model_ = nullptr;
  reading_list_download_service_ = nullptr;
  if (completion_) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(completion_), success));
  }
}

}  // namespace reading_list
