// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/public/password_import_item.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/import/import_results.h"
#import "components/url_formatter/elide_url.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/data_import/public/password_import_item_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "url/gurl.h"

namespace {

using ::password_manager::ImportEntry;

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
URLWithTitle* GetURLWithTitleForURLString(const std::string& url_string) {
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

}  // namespace

@implementation PasswordImportItem {
  /// Indicates whether favicon loading is initiated.
  BOOL _faviconLoadingInitiated;
}

+ (NSArray<PasswordImportItem*>*)passwordImportItemsFromImportResults:
    (const password_manager::ImportResults&)results {
  NSMutableArray* passwordItems = [NSMutableArray array];
  for (const ImportEntry& entry : results.displayed_entries) {
    if (entry.url.empty() && entry.username.empty()) {
      continue;
    }
    PasswordImportItem* item = [[PasswordImportItem alloc]
        initWithURL:GetURLWithTitleForURLString(entry.url)
           username:base::SysUTF8ToNSString(entry.username)
           password:base::SysUTF8ToNSString(entry.password)
             status:GetPasswordImportStatusFromImportEntryStatus(entry.status)];
    [passwordItems addObject:item];
  }
  return passwordItems;
}

- (instancetype)initWithURL:(URLWithTitle*)url
                   username:(NSString*)username
                   password:(NSString*)password
                     status:(PasswordImportStatus)status {
  self = [super init];
  if (self) {
    _url = url;
    _username = username;
    _password = password;
    _status = status;
  }
  return self;
}

- (void)loadFaviconWithUIUpdateHandler:(ProceduralBlock)handler {
  if (_faviconLoadingInitiated) {
    return;
  }
  _faviconLoadingInitiated =
      [self.faviconDataSource passwordImportItem:self
              loadFaviconAttributesWithUIHandler:handler];
}

@end
