// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/model/ios_safari_data_import_client.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/import/import_results.h"
#import "components/url_formatter/elide_url.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/safari_data_import/public/password_import_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_item_consumer.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "url/gurl.h"

namespace {

using password_manager::ImportEntry;
using password_manager::ImportResults;
using user_data_importer::ImportPreparationError;

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

// URL and its formatted string representation.
URLWithTitle* GetURLWithTitleForURLString(std::string url_string) {
  GURL url = url_formatter::FixupURL(url_string, std::string());
  if (url.is_empty()) {
    return nil;
  }
  NSString* title = base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              url));
  return [[URLWithTitle alloc] initWithURL:url title:title];
}

// Converts `ImportResults` to a list of `PasswordImportItem`s.
NSArray<PasswordImportItem*>* GetPasswordImportItemsFromImportResults(
    const ImportResults& results) {
  NSMutableArray* password_items = [NSMutableArray array];
  for (const ImportEntry& entry : results.displayed_entries) {
    if (entry.url.empty() && entry.username.empty()) {
      continue;
    }
    PasswordImportItem* item = [[PasswordImportItem alloc]
        initWithURL:GetURLWithTitleForURLString(entry.url)
           username:base::SysUTF8ToNSString(entry.username)
           password:base::SysUTF8ToNSString(entry.password)
             status:GetPasswordImportStatusFromImportEntryStatus(entry.status)];
    [password_items addObject:item];
  }
  return password_items;
}

void HandleCountOrErrorResult(id<SafariDataItemConsumer> consumer,
                              SafariDataItemType type,
                              user_data_importer::CountOrError result) {
  size_t count = 0;
  SafariDataItemImportStatus status = SafariDataItemImportStatus::kReady;
  if (result.has_value()) {
    count = result.value();
  } else if (result.error() ==
             user_data_importer::ImportPreparationError::kBlockedByPolicy) {
    status = SafariDataItemImportStatus::kBlockedByPolicy;
  }

  [consumer populateItem:[[SafariDataItem alloc] initWithType:type
                                                       status:status
                                                        count:count]];
}

}  // namespace

IOSSafariDataImportClient::IOSSafariDataImportClient() = default;
IOSSafariDataImportClient::~IOSSafariDataImportClient() = default;

void IOSSafariDataImportClient::SetSafariDataItemConsumer(
    id<SafariDataItemConsumer> consumer) {
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
  HandleCountOrErrorResult(consumer_, SafariDataItemType::kBookmarks, result);
}

void IOSSafariDataImportClient::OnHistoryReady(
    user_data_importer::CountOrError estimated_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleCountOrErrorResult(consumer_, SafariDataItemType::kHistory,
                           estimated_count);
}

void IOSSafariDataImportClient::OnPasswordsReady(
    base::expected<password_manager::ImportResults, ImportPreparationError>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t count = 0;
  SafariDataItemImportStatus status = SafariDataItemImportStatus::kReady;
  if (result.has_value()) {
    const ImportResults& results = result.value();
    conflicting_passwords_ = GetPasswordImportItemsFromImportResults(results);
    count = results.number_to_import + results.displayed_entries.size();
  } else if (result.error() == ImportPreparationError::kBlockedByPolicy) {
    status = SafariDataItemImportStatus::kBlockedByPolicy;
  }
  [consumer_ populateItem:[[SafariDataItem alloc]
                              initWithType:SafariDataItemType::kPasswords
                                    status:status
                                     count:count]];
}

void IOSSafariDataImportClient::OnPaymentCardsReady(
    user_data_importer::CountOrError result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  HandleCountOrErrorResult(consumer_, SafariDataItemType::kPayment, result);
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

base::WeakPtr<user_data_importer::SafariDataImportClient>
IOSSafariDataImportClient::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}
