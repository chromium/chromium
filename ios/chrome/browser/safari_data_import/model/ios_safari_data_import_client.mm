// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/model/ios_safari_data_import_client.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/import/import_results.h"
#import "ios/chrome/browser/safari_data_import/public/password_import_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item_consumer.h"

namespace {

using password_manager::ImportEntry;
using password_manager::ImportResults;

// Converts password_manager::ImportEntry::Status to PasswordImportStatus.
PasswordImportStatus GetPasswordImportStatusFromImportEntryStatus(
    password_manager::ImportEntry::Status status) {
  switch (status) {
    case ImportEntry::Status::NONE:
      return PasswordImportStatus::kNone;
    case ImportEntry::Status::UNKNOWN_ERROR:
      return PasswordImportStatus::kUnknownError;
    case ImportEntry::Status::MISSING_PASSWORD:
      return PasswordImportStatus::kMissingPassword;
    case ImportEntry::Status::MISSING_URL:
      return PasswordImportStatus::kMissingURL;
    case ImportEntry::Status::INVALID_URL:
      return PasswordImportStatus::kInvalidURL;
    case ImportEntry::Status::LONG_URL:
      return PasswordImportStatus::kLongUrl;
    case ImportEntry::Status::LONG_PASSWORD:
      return PasswordImportStatus::kLongPassword;
    case ImportEntry::Status::LONG_USERNAME:
      return PasswordImportStatus::kLongUsername;
    case ImportEntry::Status::CONFLICT_PROFILE:
      return PasswordImportStatus::kConflictProfile;
    case ImportEntry::Status::CONFLICT_ACCOUNT:
      return PasswordImportStatus::kConflictAccount;
    case ImportEntry::Status::LONG_NOTE:
      return PasswordImportStatus::kLongNote;
    case ImportEntry::Status::LONG_CONCATENATED_NOTE:
      return PasswordImportStatus::kLongConcatenatedNote;
    case ImportEntry::Status::VALID:
      return PasswordImportStatus::kValid;
  }
  NOTREACHED();
}

// Converts `ImportResults` to a list of `PasswordImportItem`s.
NSArray<PasswordImportItem*>* GetPasswordImportItemsFromImportResults(
    const ImportResults& results) {
  NSMutableArray* password_items = [NSMutableArray array];
  for (const ImportEntry& entry : results.displayed_entries) {
    PasswordImportItem* item = [[PasswordImportItem alloc]
        initWithURL:base::SysUTF8ToNSString(entry.url)
           username:base::SysUTF8ToNSString(entry.username)
           password:base::SysUTF8ToNSString(entry.password)
             status:GetPasswordImportStatusFromImportEntryStatus(entry.status)];
    [password_items addObject:item];
  }
  return password_items;
}

}  // namespace

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

NSArray<PasswordImportItem*>*
IOSSafariDataImportClient::GetConflictingPasswords() {
  return conflicting_passwords_;
}

NSArray<PasswordImportItem*>* IOSSafariDataImportClient::GetInvalidPasswords() {
  return invalid_passwords_;
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
  conflicting_passwords_ = GetPasswordImportItemsFromImportResults(results);
  [consumer_ populateItem:[[SafariDataItem alloc]
                              initWithType:SafariDataItemType::kPasswords
                                    status:SafariDataItemImportStatus::kReady
                                     count:results.number_to_import +
                                           results.displayed_entries.size()]];
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
  invalid_passwords_ = GetPasswordImportItemsFromImportResults(results);
  SafariDataItem* item =
      [[SafariDataItem alloc] initWithType:SafariDataItemType::kPasswords
                                    status:SafariDataItemImportStatus::kImported
                                     count:results.number_imported];
  item.invalidCount = static_cast<int>(invalid_passwords_.count);
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
