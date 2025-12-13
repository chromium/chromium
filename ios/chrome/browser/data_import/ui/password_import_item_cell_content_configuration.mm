// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/ui/password_import_item_cell_content_configuration.h"

#import "base/notreached.h"
#import "ios/chrome/browser/data_import/public/password_import_item.h"
#import "ios/chrome/browser/data_import/ui/password_import_item_cell_content_view.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

/// Returns the error message corresponding to the password import status.
NSString* GetErrorMessageForPasswordImportStatus(PasswordImportStatus status) {
  int message_id;
  switch (status) {
    case PasswordImportStatus::kUnknownError:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_UNKNOWN;
      break;
    case PasswordImportStatus::kMissingPassword:
      message_id =
          IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_MISSING_PASSWORD;
      break;
    case PasswordImportStatus::kMissingURL:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_MISSING_URL;
      break;
    case PasswordImportStatus::kInvalidURL:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_INVALID_URL;
      break;
    case PasswordImportStatus::kLongUrl:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_LONG_URL;
      break;
    case PasswordImportStatus::kLongPassword:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_LONG_PASSWORD;
      break;
    case PasswordImportStatus::kLongUsername:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_LONG_USERNAME;
      break;
    case PasswordImportStatus::kLongNote:
    case PasswordImportStatus::kLongConcatenatedNote:
      message_id = IDS_IOS_SAFARI_IMPORT_INVALID_PASSWORD_REASON_LONG_NOTE;
      break;
    case PasswordImportStatus::kConflictProfile:
    case PasswordImportStatus::kConflictAccount:
    case PasswordImportStatus::kNone:
    case PasswordImportStatus::kValid:
      NOTREACHED();
  }
  return l10n_util::GetNSString(message_id);
}

}  // namespace

#pragma mark - PasswordImportItemCellContentConfiguration

@implementation PasswordImportItemCellContentConfiguration

+ (instancetype)cellConfigurationForMaskPassword:(PasswordImportItem*)item {
  return [[PasswordImportItemCellContentConfiguration alloc]
          initPrivateWithURL:item.url.title
                    username:item.username
                     message:kMaskedPassword
      shouldHighlightMessage:NO];
}

+ (instancetype)cellConfigurationForUnmaskPassword:(PasswordImportItem*)item {
  return [[PasswordImportItemCellContentConfiguration alloc]
          initPrivateWithURL:item.url.title
                    username:item.username
                     message:item.password
      shouldHighlightMessage:NO];
}

+ (instancetype)cellConfigurationForErrorMessage:(PasswordImportItem*)item {
  return [[PasswordImportItemCellContentConfiguration alloc]
          initPrivateWithURL:item.url.title
                    username:item.username
                     message:GetErrorMessageForPasswordImportStatus(item.status)
      shouldHighlightMessage:YES];
}

#pragma mark - Private initializer

/// Private initializer.
- (instancetype)initPrivateWithURL:(NSString*)url
                          username:(NSString*)username
                           message:(NSString*)message
            shouldHighlightMessage:(BOOL)shouldHighlightMessage {
  self = [super init];
  if (self) {
    _URL = url;
    _username = username;
    _message = message;
    _isMessageHighlighted = shouldHighlightMessage;
  }
  return self;
}

#pragma mark - UIContentConfiguration

- (id<UIContentView>)makeContentView {
  return [[PasswordImportItemCellContentView alloc] initWithConfiguration:self];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  /// Password import cell looks the same across different states.
  return self;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  PasswordImportItemCellContentConfiguration* newCopy =
      [[PasswordImportItemCellContentConfiguration alloc]
              initPrivateWithURL:self.URL
                        username:self.username
                         message:self.message
          shouldHighlightMessage:self.isMessageHighlighted];
  newCopy.faviconAttributes = self.faviconAttributes;
  return newCopy;
}

@end
