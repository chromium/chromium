// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_cells_constants.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/ui/avatar/ai_tier_avatar_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Size of the error icon image.
constexpr CGFloat kErrorIconImageSize = 22.;

// Point size of enterprise icon in the bottom view.
constexpr CGFloat kEnterpriseIconPointSize = 20;
}  // namespace

@implementation TableViewAccountItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [LegacyTableViewCell class];
    _mode = TableViewAccountModeEnabled;
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(LegacyTableViewCell*)cell {
  [super configureCell:cell];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];

  configuration.title = self.text;
  configuration.titleNumberOfLines = 1;
  configuration.titleLineBreakMode = NSLineBreakByTruncatingTail;
  configuration.subtitle = self.detailText;
  configuration.subtitleNumberOfLines = 1;
  configuration.subtitleLineBreakMode = NSLineBreakByTruncatingTail;

  BOOL showsAITierRing = NO;
  switch (self.detailImage) {
    case TableViewAccountDetailImage::kNone:
      break;
    case TableViewAccountDetailImage::kError: {
      ImageContentConfiguration* trailingImageConfiguration =
          [[ImageContentConfiguration alloc] init];
      trailingImageConfiguration.image =
          SymbolWithPointSize(SymbolErrorCircleFill, kErrorIconImageSize);
      trailingImageConfiguration.imageSize =
          CGSizeMake(kTableViewIconImageSize, kTableViewIconImageSize);
      trailingImageConfiguration.imageTintColor =
          [UIColor colorNamed:kRed500Color];
      configuration.trailingConfiguration = trailingImageConfiguration;
      break;
    }
    case TableViewAccountDetailImage::kManaged: {
      ImageContentConfiguration* trailingImageConfiguration =
          [[ImageContentConfiguration alloc] init];
      trailingImageConfiguration.image = SymbolWithPalette(
          SymbolWithPointSize(SymbolEnterprise, kEnterpriseIconPointSize),
          @[ [UIColor colorNamed:kStaticGrey600Color] ]);
      configuration.trailingConfiguration = trailingImageConfiguration;
      break;
    }
    case TableViewAccountDetailImage::kAITierRing:
      showsAITierRing = YES;
      break;
  }

  CHECK(self.image);
  AITierAvatarContentConfiguration* avatarConfiguration =
      [[AITierAvatarContentConfiguration alloc]
          initWithAvatarImage:self.image
                     diameter:kTableViewIconImageSize
                     sizeType:AITierAvatarSizeType::kFullView
              showsAITierRing:showsAITierRing];

  configuration.leadingConfiguration = avatarConfiguration;
  cell.contentConfiguration = configuration;

  cell.userInteractionEnabled = self.mode == TableViewAccountModeEnabled;

  cell.accessibilityValue =
      (self.detailImage == TableViewAccountDetailImage::kError)
          ? l10n_util::GetNSString(
                IDS_IOS_ITEM_ACCOUNT_ERROR_BADGE_ACCESSIBILITY_HINT)
          : nil;

  cell.accessibilityTraits |= UIAccessibilityTraitButton;

  BOOL isManaged = self.detailImage == TableViewAccountDetailImage::kManaged;

  // When not set, the screen readers will read this cell as "text, detailText".
  // Add a custom accessibility label for managed accounts to append "managed by
  // your organization" so that the screen readers read this cell as "text,
  // detailText, managed by your organization".
  if (isManaged) {
    cell.accessibilityLabel =
        self.text && self.detailText
            ? l10n_util::GetNSStringF(
                  IDS_IOS_SIGNIN_ACCOUNT_PICKER_CHOOSE_ACCOUNT_ITEM_DESCRIPTION_WITH_NAME_AND_EMAIL_MANAGED,
                  base::SysNSStringToUTF16(self.text),
                  base::SysNSStringToUTF16(self.detailText))
            : l10n_util::GetNSStringF(
                  IDS_IOS_SIGNIN_ACCOUNT_PICKER_CHOOSE_ACCOUNT_ITEM_DESCRIPTION_WITH_EMAIL_MANAGED,
                  base::SysNSStringToUTF16(self.text ? self.text
                                                     : self.detailText));
  } else {
    cell.accessibilityLabel = configuration.accessibilityLabel;
  }
}

- (LegacyTableViewCell*)cellForTableView:(UITableView*)tableView {
  [TableViewCellContentConfiguration legacyRegisterCellForTableView:tableView];
  return
      [TableViewCellContentConfiguration legacyDequeueTableViewCell:tableView];
}

#pragma mark - Private

// Returns the main text to display in the item. The name if available otherwise
// the email.
- (NSString*)text {
  if (self.name) {
    return self.name;
  }
  return self.email;
}

// Returns the secondary text to display in the item if any. That is, the email
// if there is a name, otherwise nothing.
- (NSString*)detailText {
  if (self.name) {
    return self.email;
  }
  return nil;
}

@end
