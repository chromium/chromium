// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_credential.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kMaskedPasswordTitle = @"••••••••";

@interface ManualFillCredentialItem ()

// The credential for this item.
@property(nonatomic, strong, readonly) ManualFillCredential* credential;

// The cell won't show a title (site name) label if it is connected to the
// previous password item.
@property(nonatomic, assign) BOOL isConnectedToPreviousItem;

// The separator line won't show if it is connected to the next password item.
@property(nonatomic, assign) BOOL isConnectedToNextItem;

// The delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentInjector>
    contentInjector;

@end

@implementation ManualFillCredentialItem

- (instancetype)initWithCredential:(ManualFillCredential*)credential
         isConnectedToPreviousItem:(BOOL)isConnectedToPreviousItem
             isConnectedToNextItem:(BOOL)isConnectedToNextItem
                   contentInjector:
                       (id<ManualFillContentInjector>)contentInjector {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _credential = credential;
    _isConnectedToPreviousItem = isConnectedToPreviousItem;
    _isConnectedToNextItem = isConnectedToNextItem;
    _contentInjector = contentInjector;
    self.cellClass = [ManualFillPasswordCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillPasswordCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithCredential:self.credential
      isConnectedToPreviousCell:self.isConnectedToPreviousItem
          isConnectedToNextCell:self.isConnectedToNextItem
                contentInjector:self.contentInjector];
}

- (const GURL&)faviconURL {
  return self.credential.URL;
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.credential.URL.spec());
}

@end

namespace {

// The multiplier for the base system spacing at the top margin for connected
// cells.
static const CGFloat TopSystemSpacingMultiplierForConnectedCell = 1.28;

// When no extra multiplier is required.
static const CGFloat NoMultiplier = 1.0;

}  // namespace

@interface ManualFillPasswordCell ()

// The credential this cell is showing.
@property(nonatomic, strong) ManualFillCredential* credential;

// The dynamic constraints for all the lines (i.e. not set in createView).
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* dynamicConstraints;

// The constraints for the visible favicon.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* faviconContraints;

// The favicon for the credential.
@property(nonatomic, strong) FaviconView* faviconView;

// The label with the site name and host.
@property(nonatomic, strong) UILabel* siteNameLabel;

// A button showing the username, or "No Username".
@property(nonatomic, strong) UIButton* usernameButton;

// A button showing "••••••••" to resemble a password.
@property(nonatomic, strong) UIButton* passwordButton;

// Separator line between cells, if needed.
@property(nonatomic, strong) UIView* grayLine;

// The delegate in charge of processing the user actions in this cell.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;

@end

@implementation ManualFillPasswordCell

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];
  [NSLayoutConstraint deactivateConstraints:self.faviconContraints];
  self.faviconView.hidden = YES;

  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];

  self.siteNameLabel.text = @"";
  [self.faviconView configureWithAttributes:nil];

  [self.usernameButton setTitle:@"" forState:UIControlStateNormal];
  self.usernameButton.enabled = YES;
  [self.usernameButton setTitleColor:[UIColor colorNamed:kTextPrimaryColor]
                            forState:UIControlStateNormal];

  [self.passwordButton setTitle:@"" forState:UIControlStateNormal];
  self.passwordButton.accessibilityLabel = nil;
  self.passwordButton.hidden = NO;

  self.credential = nil;

  self.grayLine.hidden = NO;
}

- (void)setUpWithCredential:(ManualFillCredential*)credential
    isConnectedToPreviousCell:(BOOL)isConnectedToPreviousCell
        isConnectedToNextCell:(BOOL)isConnectedToNextCell
              contentInjector:(id<ManualFillContentInjector>)contentInjector {
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.contentInjector = contentInjector;
  self.credential = credential;

  NSMutableArray<UIView*>* verticalLeadViews = [[NSMutableArray alloc] init];

  if (isConnectedToPreviousCell) {
    self.siteNameLabel.hidden = YES;
    self.faviconView.hidden = YES;
  } else {
    NSMutableAttributedString* attributedString =
        [[NSMutableAttributedString alloc]
            initWithString:credential.siteName ? credential.siteName : @""
                attributes:@{
                  NSForegroundColorAttributeName :
                      [UIColor colorNamed:kTextPrimaryColor],
                  NSFontAttributeName :
                      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
                }];
    if (credential.host && credential.host.length &&
        ![credential.host isEqualToString:credential.siteName]) {
      NSString* hostString =
          [NSString stringWithFormat:@" –– %@", credential.host];
      NSDictionary* attributes = @{
        NSForegroundColorAttributeName :
            [UIColor colorNamed:kTextSecondaryColor],
        NSFontAttributeName :
            [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
      };
      NSAttributedString* hostAttributedString =
          [[NSAttributedString alloc] initWithString:hostString
                                          attributes:attributes];
      [attributedString appendAttributedString:hostAttributedString];
    }
    self.siteNameLabel.attributedText = attributedString;
    [verticalLeadViews addObject:self.siteNameLabel];
    self.siteNameLabel.hidden = NO;
    self.faviconView.hidden = NO;
  }

  if (credential.username.length) {
    [self.usernameButton setTitle:credential.username
                         forState:UIControlStateNormal];
  } else {
    NSString* titleString =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_USERNAME);
    [self.usernameButton setTitle:titleString forState:UIControlStateNormal];
    [self.usernameButton setTitleColor:[UIColor colorNamed:kTextSecondaryColor]
                              forState:UIControlStateNormal];
    self.usernameButton.enabled = NO;
  }
  [verticalLeadViews addObject:self.usernameButton];

  if (credential.password.length) {
    [self.passwordButton setTitle:kMaskedPasswordTitle
                         forState:UIControlStateNormal];
    self.passwordButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL);
    [verticalLeadViews addObject:self.passwordButton];
    self.passwordButton.hidden = NO;
  } else {
    self.passwordButton.hidden = YES;
  }

  if (isConnectedToNextCell) {
    self.grayLine.hidden = YES;
  }

  CGFloat topSystemSpacingMultiplier =
      isConnectedToPreviousCell ? TopSystemSpacingMultiplierForConnectedCell
                                : TopSystemSpacingMultiplier;
  CGFloat bottomSystemSpacingMultiplier =
      isConnectedToNextCell ? NoMultiplier : BottomSystemSpacingMultiplier;

  self.dynamicConstraints = [[NSMutableArray alloc] init];
  AppendVerticalConstraintsSpacingForViews(
      self.dynamicConstraints, verticalLeadViews, self.contentView,
      topSystemSpacingMultiplier, bottomSystemSpacingMultiplier);
  [NSLayoutConstraint activateConstraints:self.dynamicConstraints];
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.credential.URL.spec());
}

- (void)configureWithFaviconAttributes:(FaviconAttributes*)attributes {
  if (attributes.faviconImage) {
    self.faviconView.hidden = NO;
    [NSLayoutConstraint activateConstraints:self.faviconContraints];
    [self.faviconView configureWithAttributes:attributes];
    return;
  }
  [NSLayoutConstraint deactivateConstraints:self.faviconContraints];
  self.faviconView.hidden = YES;
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.selectionStyle = UITableViewCellSelectionStyleNone;

  self.grayLine = CreateGraySeparatorForContainer(self.contentView);
  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];

  self.faviconView = [[FaviconView alloc] init];
  self.faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  self.faviconView.clipsToBounds = YES;
  self.faviconView.hidden = YES;
  [self.contentView addSubview:self.faviconView];
  self.faviconContraints = @[
    [self.faviconView.widthAnchor constraintEqualToConstant:gfx::kFaviconSize],
    [self.faviconView.heightAnchor
        constraintEqualToAnchor:self.faviconView.widthAnchor],
  ];

  self.siteNameLabel = CreateLabel();
  self.siteNameLabel.translatesAutoresizingMaskIntoConstraints = NO;
  self.siteNameLabel.adjustsFontForContentSizeCategory = YES;
  [self.contentView addSubview:self.siteNameLabel];

  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ self.faviconView, self.siteNameLabel ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.spacing = UIStackViewSpacingUseSystem;
  stackView.alignment = UIStackViewAlignmentCenter;

  [self.contentView addSubview:stackView];

  AppendHorizontalConstraintsForViews(staticConstraints, @[ stackView ],
                                      self.contentView,
                                      kButtonHorizontalMargin);

  self.usernameButton = CreateChipWithSelectorAndTarget(
      @selector(userDidTapUsernameButton:), self);
  [self.contentView addSubview:self.usernameButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.usernameButton ], self.grayLine,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.passwordButton = CreateChipWithSelectorAndTarget(
      @selector(userDidTapPasswordButton:), self);
  [self.contentView addSubview:self.passwordButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.passwordButton ], self.grayLine,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  [NSLayoutConstraint activateConstraints:staticConstraints];
}

- (void)userDidTapUsernameButton:(UIButton*)button {
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Password_SelectUsername"));
  [self.contentInjector userDidPickContent:self.credential.username
                             passwordField:NO
                             requiresHTTPS:NO];
}

- (void)userDidTapPasswordButton:(UIButton*)button {
  if (![self.contentInjector canUserInjectInPasswordField:YES
                                            requiresHTTPS:YES]) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Password_SelectPassword"));
  [self.contentInjector userDidPickContent:self.credential.password
                             passwordField:YES
                             requiresHTTPS:YES];
}

@end
