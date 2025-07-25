// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/model/ios_safari_data_import_client.h"

#import "base/notreached.h"
#import "components/password_manager/core/browser/import/import_results.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item_consumer.h"

IOSSafariDataImportClient::IOSSafariDataImportClient() = default;
IOSSafariDataImportClient::~IOSSafariDataImportClient() = default;

void IOSSafariDataImportClient::SetSafariDataItemConsumer(
    id<SafariDataItemConsumer> consumer) {
  consumer_ = consumer;
}

void IOSSafariDataImportClient::RegisterCallbackOnImportFailure(
    ImportFailureCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  failure_callbacks_subscription_ = failure_callbacks_.Add(std::move(callback));
}

#pragma mark - IOSSafariDataImportClient

void IOSSafariDataImportClient::OnTotalFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  failure_callbacks_.Notify();
}

void IOSSafariDataImportClient::OnBookmarksReady(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [consumer_ populateItem:[[SafariDataItem alloc]
                              initWithType:SafariDataItemType::kBookmarks
                                    status:SafariDataItemImportStatus::kReady
                                     count:count]];
}

void IOSSafariDataImportClient::OnHistoryReady(
    size_t estimated_count,
    std::vector<std::u16string> profiles) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [consumer_ populateItem:[[SafariDataItem alloc]
                              initWithType:SafariDataItemType::kHistory
                                    status:SafariDataItemImportStatus::kReady
                                     count:estimated_count]];
}

void IOSSafariDataImportClient::OnPasswordsReady(
    const password_manager::ImportResults& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/420703283): Extrapolate password details.
  [consumer_ populateItem:[[SafariDataItem alloc]
                              initWithType:SafariDataItemType::kPasswords
                                    status:SafariDataItemImportStatus::kReady
                                     count:results.number_to_import]];
}

void IOSSafariDataImportClient::OnPaymentCardsReady(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [consumer_ populateItem:[[SafariDataItem alloc]
                              initWithType:SafariDataItemType::kPayment
                                    status:SafariDataItemImportStatus::kReady
                                     count:count]];
}

void IOSSafariDataImportClient::OnBookmarksImported(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [consumer_ populateItem:[[SafariDataItem alloc]
                              initWithType:SafariDataItemType::kBookmarks
                                    status:SafariDataItemImportStatus::kImported
                                     count:count]];
}

void IOSSafariDataImportClient::OnHistoryImported(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [consumer_ populateItem:[[SafariDataItem alloc]
                              initWithType:SafariDataItemType::kHistory
                                    status:SafariDataItemImportStatus::kImported
                                     count:count]];
}

void IOSSafariDataImportClient::OnPasswordsImported(
    const password_manager::ImportResults& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/420703283): Extrapolate password details.
  SafariDataItem* item =
      [[SafariDataItem alloc] initWithType:SafariDataItemType::kPasswords
                                    status:SafariDataItemImportStatus::kImported
                                     count:results.number_imported];
  item.invalidCount = static_cast<int>(results.displayed_entries.size());
  [consumer_ populateItem:item];
}

void IOSSafariDataImportClient::OnPaymentCardsImported(size_t count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [consumer_ populateItem:[[SafariDataItem alloc]
                              initWithType:SafariDataItemType::kPayment
                                    status:SafariDataItemImportStatus::kImported
                                     count:count]];
}

base::WeakPtr<SafariDataImportClient> IOSSafariDataImportClient::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}
