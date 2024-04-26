// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_manager_view_controller_items.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/span.h"
#import "base/ranges/algorithm.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#pragma mark - PasswordFormContentCell

@interface PasswordFormContentCell ()

// The title, displayed on top.
@property(nonatomic, strong, readonly) UILabel* titleLabel;

// Optional detail text, displayed below the title.
@property(nonatomic, strong, readonly) UILabel* detailLabel;

// The favicon view, left-aligned.
@property(nonatomic, strong, readonly)
    FaviconContainerView* faviconContainerView;

// Icon indicating the data is local-only, right-aligned.
@property(nonatomic, strong, readonly) UIImageView* localOnlyIcon;

// The page URL used to asynchronously load the icon.
@property(nonatomic, assign) GURL faviconPageURL;

@end

@implementation PasswordFormContentCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (!self) {
    return nil;
  }

  _faviconTypeForMetrics = FaviconTypeNotLoaded;
  _titleLabel = [[UILabel alloc] init];
  _detailLabel = [[UILabel alloc] init];
  _faviconContainerView = [[FaviconContainerView alloc] init];
  UIImage* cloudSlashedImage =
      CustomSymbolWithPointSize(kCloudSlashSymbol, kCloudSlashSymbolPointSize);
  _localOnlyIcon = [[UIImageView alloc] initWithImage:cloudSlashedImage];
  _localOnlyIcon.tintColor = CloudSlashTintColor();
  [_localOnlyIcon setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisHorizontal];
  [_localOnlyIcon setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisVertical];
  [_localOnlyIcon
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_localOnlyIcon
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  _localOnlyIcon.accessibilityIdentifier = kLocalOnlyPasswordIconID;

  _titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _detailLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _detailLabel.adjustsFontForContentSizeCategory = YES;
  _detailLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _titleLabel,
    _detailLabel,
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;

  UIStackView* horizontalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ verticalStack, _localOnlyIcon ]];
  horizontalStack.axis = UILayoutConstraintAxisHorizontal;
  horizontalStack.spacing = kTableViewSubViewHorizontalSpacing;
  horizontalStack.distribution = UIStackViewDistributionFill;
  horizontalStack.alignment = UIStackViewAlignmentCenter;

  _faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
  _localOnlyIcon.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:_faviconContainerView];
  [self.contentView addSubview:horizontalStack];

  NSLayoutConstraint* heightConstraint = [self.contentView.heightAnchor
      constraintGreaterThanOrEqualToConstant:kChromeTableViewCellHeight];
  // Don't set the priority to required to avoid clashing with the estimated
  // height.
  heightConstraint.priority = UILayoutPriorityRequired - 1;

  [NSLayoutConstraint activateConstraints:@[
    heightConstraint,
    [_faviconContainerView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing],
    [_faviconContainerView.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [horizontalStack.leadingAnchor
        constraintEqualToAnchor:_faviconContainerView.trailingAnchor
                       constant:kTableViewSubViewHorizontalSpacing],
    [horizontalStack.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [horizontalStack.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-kTableViewHorizontalSpacing],
    [horizontalStack.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
    [horizontalStack.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.bottomAnchor
                                    constant:
                                        -kTableViewTwoLabelsCellVerticalSpacing]

  ]];

  return self;
}

- (void)loadFavicon:(id<TableViewFaviconDataSource>)faviconDataSource {
  DCHECK(!self.faviconPageURL.is_empty()) << "Cell not configured yet";

  __weak __typeof(self) weakSelf = self;
  GURL requestedURL = self.faviconPageURL;
  [faviconDataSource faviconForPageURL:[[CrURL alloc] initWithGURL:requestedURL]
                            completion:^(FaviconAttributes* attributes) {
                              DCHECK(attributes);

                              __typeof(self) strongSelf = weakSelf;
                              if (!strongSelf) {
                                return;
                              }

                              if (strongSelf.faviconPageURL != requestedURL) {
                                // The favicon doesn't fit anymore, an item with
                                // a different URL reused the cell.
                                return;
                              }

                              strongSelf.faviconTypeForMetrics =
                                  attributes.faviconImage ? FaviconTypeImage
                                                          : FaviconTypeMonogram;
                              [self.faviconContainerView.faviconView
                                  configureWithAttributes:attributes];
                            }];
}

// TODO(crbug.com/40880506): If FaviconContainerView exposed its state, the
// implementation of this readonly property could use that rather than an ivar.
- (void)setFaviconTypeForMetrics:(FaviconType)faviconTypeForMetrics {
  _faviconTypeForMetrics = faviconTypeForMetrics;
}

- (NSString*)accessibilityLabel {
  NSString* label = _titleLabel.text;
  if (_detailLabel.text.length) {
    label = [NSString stringWithFormat:@"%@, %@", label, _detailLabel.text];
  }
  if (!_localOnlyIcon.hidden) {
    label = [NSString
        stringWithFormat:@"%@, %@", label,
                         l10n_util::GetNSString(
                             IDS_IOS_LOCAL_PASSWORD_ACCESSIBILITY_LABEL)];
  }
  return label;
}

- (NSString*)accessibilityIdentifier {
  return _detailLabel.text.length
             ? [NSString stringWithFormat:@"%@, %@", _titleLabel.text,
                                          _detailLabel.text]
             : _titleLabel.text;
}

- (BOOL)isAccessibilityElement {
  return YES;
}

@end

#pragma mark - AffiliatedGroupTableViewItem

@implementation AffiliatedGroupTableViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PasswordFormContentCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];

  PasswordFormContentCell* cell =
      base::apple::ObjCCastStrict<PasswordFormContentCell>(tableCell);
  cell.titleLabel.text = self.title;
  // Title might be a URL, use "...oo.bar.com", not "fooooooooo..." if too big.
  cell.titleLabel.lineBreakMode = NSLineBreakByTruncatingHead;
  cell.detailLabel.text = self.detailText;
  cell.detailLabel.hidden = !cell.detailLabel.text.length;
  // TODO(crbug.com/40860113): Use AffiliationGroup::GetIconURL() instead.
  cell.faviconPageURL = self.affiliatedGroup.GetCredentials().begin()->GetURL();
  cell.localOnlyIcon.hidden = !self.showLocalOnlyIcon;
  if (styler.cellTitleColor) {
    cell.titleLabel.textColor = styler.cellTitleColor;
  }
}

- (NSString*)title {
  return base::SysUTF8ToNSString(self.affiliatedGroup.GetDisplayName());
}

- (NSString*)detailText {
  const int nbAccounts = self.affiliatedGroup.GetCredentials().size();
  return nbAccounts > 1 ? l10n_util::GetNSStringF(
                              IDS_IOS_SETTINGS_PASSWORDS_NUMBER_ACCOUNT,
                              base::NumberToString16(nbAccounts))
                        : @"";
}

@end

#pragma mark - BlockedSiteTableViewItem

@implementation BlockedSiteTableViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PasswordFormContentCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  CHECK(self.credential.blocked_by_user);
  [super configureCell:tableCell withStyler:styler];

  PasswordFormContentCell* cell =
      base::apple::ObjCCastStrict<PasswordFormContentCell>(tableCell);
  cell.titleLabel.text = self.title;
  // Title is a URL, use "...oo.bar.com", not "fooooooooo..." if too big.
  cell.titleLabel.lineBreakMode = NSLineBreakByTruncatingHead;
  cell.detailLabel.hidden = !cell.detailLabel.text.length;
  cell.faviconPageURL = self.credential.GetURL();
  cell.localOnlyIcon.hidden = YES;
  if (styler.cellTitleColor) {
    cell.titleLabel.textColor = styler.cellTitleColor;
  }
}

- (NSString*)title {
  return base::SysUTF8ToNSString(
      password_manager::GetShownOrigin(self.credential));
  ;
}

@end
