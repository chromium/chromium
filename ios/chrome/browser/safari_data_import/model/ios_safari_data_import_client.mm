// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/model/ios_safari_data_import_client.h"

#import "components/password_manager/core/browser/import/import_results.h"
#import "ios/chrome/browser/data_import/public/import_data_item.h"
#import "ios/chrome/browser/data_import/public/import_data_item_consumer.h"
#import "ios/chrome/browser/data_import/public/password_import_item.h"

namespace {

using ::password_manager::ImportResults;
using ::user_data_importer::ImportPreparationError;

void HandleCountOrErrorResult(id<ImportDataItemConsumer> consumer,
                              ImportDataItemType type,
                              user_data_importer::CountOrError result) {
  size_t count = 0;
  ImportDataItemImportStatus status = ImportDataItemImportStatus::kReady;
  if (result.has_value()) {
    count = result.value();
  } else if (result.error() ==
             user_data_importer::ImportPreparationError::kBlockedByPolicy) {
    status = ImportDataItemImportStatus::kBlockedByPolicy;
  }

  [consumer populateItem:[[ImportDataItem alloc] initWithType:type
                                                       status:status
                                                        count:count]];
}

}  // namespace

IOSSafariDataImportClient::IOSSafariDataImportClient() = default;
IOSSafariDataImportClient::~IOSSafariDataImportClient() = default;

void IOSSafariDataImportClient::SetImportDataItemConsumer(
    id<ImportDataItemConsumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  consumer_ = consumer;
}

void IOSSafariDataImportClient::RegisterCallbackOnImportFailure(
    ImportFailureCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  failure_callbacks_subscription_ = failure_callbacks_.Add(std::move(callback));
}

NSArray<PasswordImportItem*>*
IOSSafariDataImportClient::GetConflictingPasswords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return conflicting_passwords_;
}

NSArray<PasswordImportItem*>* IOSSafariDataImportClient::GetInvalidPasswords() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return invalid_passwords_;
}

#pragma mark - IOSSafariDataImportClient

void IOSSafariDataImportClient::OnTotalFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  failure_callbacks_.Notify();
}

void IOSSafariDataImportClient::OnBookmarksReady(
    user_data_importer::CountOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleCountOrErrorResult(consumer_, ImportDataItemType::kBookmarks, result);
}

void IOSSafariDataImportClient::OnHistoryReady(
    user_data_importer::CountOrError estimated_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleCountOrErrorResult(consumer_, ImportDataItemType::kHistory,
                           estimated_count);
}

void IOSSafariDataImportClient::OnPasswordsReady(
    base::expected<password_manager::ImportResults, ImportPreparationError>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t count = 0;
  ImportDataItemImportStatus status = ImportDataItemImportStatus::kReady;
  if (result.has_value()) {
    const ImportResults& results = result.value();
    conflicting_passwords_ =
        [PasswordImportItem passwordImportItemsFromImportResults:results];
    count = results.number_to_import + results.displayed_entries.size();
  } else if (result.error() == ImportPreparationError::kBlockedByPolicy) {
    status = ImportDataItemImportStatus::kBlockedByPolicy;
  }
  [consumer_ populateItem:[[ImportDataItem alloc]
                              initWithType:ImportDataItemType::kPasswords
                                    status:status
                                     count:count]];
}

void IOSSafariDataImportClient::OnPaymentCardsReady(
    user_data_importer::CountOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleCountOrErrorResult(consumer_, ImportDataItemType::kPayment, result);
}

void IOSSafariDataImportClient::OnBookmarksImported(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [consumer_ populateItem:[[ImportDataItem alloc]
                              initWithType:ImportDataItemType::kBookmarks
                                    status:ImportDataItemImportStatus::kImported
                                     count:count]];
}

void IOSSafariDataImportClient::OnHistoryImported(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [consumer_ populateItem:[[ImportDataItem alloc]
                              initWithType:ImportDataItemType::kHistory
                                    status:ImportDataItemImportStatus::kImported
                                     count:count]];
}

void IOSSafariDataImportClient::OnPasswordsImported(
    const password_manager::ImportResults& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  invalid_passwords_ =
      [PasswordImportItem passwordImportItemsFromImportResults:results];
  ImportDataItem* item =
      [[ImportDataItem alloc] initWithType:ImportDataItemType::kPasswords
                                    status:ImportDataItemImportStatus::kImported
                                     count:results.number_imported];
  item.invalidCount = static_cast<int>(invalid_passwords_.count);
  [consumer_ populateItem:item];
}

void IOSSafariDataImportClient::OnPaymentCardsImported(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [consumer_ populateItem:[[ImportDataItem alloc]
                              initWithType:ImportDataItemType::kPayment
                                    status:ImportDataItemImportStatus::kImported
                                     count:count]];
}

base::WeakPtr<user_data_importer::SafariDataImportClient>
IOSSafariDataImportClient::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}
