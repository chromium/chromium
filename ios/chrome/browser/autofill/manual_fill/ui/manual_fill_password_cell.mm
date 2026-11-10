// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_password_cell.h"

#import "base/strings/sys_string_conversions.h"
#import "components/webauthn/ios/features.h"
#import "components/webauthn/ios/passkey_suggestion_utils.h"
#import "ios/chrome/browser/autofill/manual_fill/model/manual_fill_credential.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_cell_utils.h"
#import "ios/chrome/browser/autofill/manual_fill/ui/manual_fill_password_cell_mutator.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface ManualFillCredentialItem ()

// The credential for this item.
@property(nonatomic, strong, readonly) ManualFillCredential* credential;

// The mutator for user actions in the cell.
@property(nonatomic, weak, readonly) id<ManualFillPasswordCellMutator> mutator;

// The UIActions that should be available from the cell's overflow menu button.
@property(nonatomic, strong) NSArray<UIAction*>* menuActions;

// The part of the cell's accessibility label that is used to indicate the index
// at which the password represented by this item is positioned in the list of
// passwords to show.
@property(nonatomic, copy) NSString* cellIndexAccessibilityLabel;

@end

@implementation ManualFillCredentialItem {
  // The 0-based index at which the password is in the list of passwords to
  // show.
  NSInteger _cellIndex;

  // If `YES`, autofill button is shown for the item.
  BOOL _showAutofillFormButton;
}

- (instancetype)initWithCredential:(ManualFillCredential*)credential
                           mutator:(id<ManualFillPasswordCellMutator>)mutator
                       menuActions:(NSArray<UIAction*>*)menuActions
                         cellIndex:(NSInteger)cellIndex
       cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
            showAutofillFormButton:(BOOL)showAutofillFormButton
                    credentialType:(ManualFillCredentialType)credentialType {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _credential = credential;
    _mutator = mutator;
    _menuActions = menuActions;
    _cellIndex = cellIndex;
    _cellIndexAccessibilityLabel = [cellIndexAccessibilityLabel copy];
    _showAutofillFormButton = showAutofillFormButton;
    _credentialType = credentialType;
    self.cellClass = [ManualFillPasswordCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillPasswordCell*)cell {
  [super configureCell:cell];
  [cell setUpWithCredential:self.credential
                          mutator:self.mutator
                      menuActions:self.menuActions
                        cellIndex:_cellIndex
      cellIndexAccessibilityLabel:_cellIndexAccessibilityLabel
           showAutofillFormButton:_showAutofillFormButton
                   credentialType:self.credentialType];
}

- (const GURL&)faviconURL {
  return self.credential.URL;
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.credential.URL.spec());
}

- (NSString*)username {
  return self.credential.username;
}

@end

namespace {

// The size that the favicon should have.
constexpr CGFloat kFaviconContainterViewSize = 30;

}  // namespace

@interface ManualFillPasswordCell ()

// The credential this cell is showing.
@property(nonatomic, strong) ManualFillCredential* credential;

// The dynamic constraints for all the lines (i.e. not set in createView).
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* dynamicConstraints;

// The view displayed at the top the cell containing the favicon, the site name
// and an overflow button.
@property(nonatomic, strong) UIView* headerView;

// The favicon for the credential.
@property(nonatomic, strong) FaviconContainerView* faviconContainerView;

// The label with the site name and host.
@property(nonatomic, strong) UILabel* siteNameLabel;

// The menu button displayed in the cell's header.
@property(nonatomic, strong) UIButton* overflowMenuButton;

// A button showing the username, or "No Username".
@property(nonatomic, strong) UIButton* usernameButton;

// A button showing "••••••••" to resemble a password.
@property(nonatomic, strong) UIButton* passwordButton;

// Separator line. Used to delimit the header from the rest of the cell.
@property(nonatomic, strong) UIView* grayLine;

// The mutator in charge of processing user actions in this cell.
@property(nonatomic, weak) id<ManualFillPasswordCellMutator> mutator;

// Layout guide for the cell's content.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

// Button to autofill the current form with the credential's data.
@property(nonatomic, strong) UIButton* autofillFormButton;

@end

@implementation ManualFillPasswordCell {
  // The 0-based index at which the password is in the list of passwords to
  // show.
  NSInteger _cellIndex;

  // UIImageView for the custom symbol that replaces the favicon in the cell.
  // Stays nil if not needed.
  UIImageView* _customSymbolImageView;

  // The type of credential.
  ManualFillCredentialType _credentialType;
}

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];

  [_customSymbolImageView removeFromSuperview];
  _customSymbolImageView = nil;

  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];

  self.siteNameLabel.text = @"";
  [self setUpFaviconViewWithAttributes:nil];

  [self.usernameButton setTitle:@"" forState:UIControlStateNormal];
  self.usernameButton.enabled = YES;
  [self.usernameButton setTitleColor:[UIColor colorNamed:kTextPrimaryColor]
                            forState:UIControlStateNormal];

  [self.passwordButton setTitle:@"" forState:UIControlStateNormal];
  self.passwordButton.accessibilityLabel = nil;
  self.passwordButton.hidden = NO;

  self.credential = nil;
  self.mutator = nil;

  self.grayLine.hidden = NO;
}

- (void)setUpWithCredential:(ManualFillCredential*)credential
                        mutator:(id<ManualFillPasswordCellMutator>)mutator
                    menuActions:(NSArray<UIAction*>*)menuActions
                      cellIndex:(NSInteger)cellIndex
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
         showAutofillFormButton:(BOOL)showAutofillFormButton
                 credentialType:(ManualFillCredentialType)credentialType {
  _cellIndex = cellIndex;
  _credentialType = credentialType;

  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.mutator = mutator;
  self.credential = credential;

  // Holds the views whose leading anchor is constrained relative to the cell's
  // leading anchor.
  std::vector<ManualFillCellView> verticalLeadViews;

  // Header.
  NSAttributedString* attributedText =
      CreateHeaderAttributedString([self cellTitle], [self cellSubtitle]);
  self.siteNameLabel.attributedText = attributedText;
  self.siteNameLabel.numberOfLines = 0;
  NSString* accessibilityLabel =
      [NSString stringWithFormat:@"%@, %@", cellIndexAccessibilityLabel,
                                 attributedText.string];
  GiveAccessibilityContextToCellAndButton(
      self.contentView, self.overflowMenuButton, self.autofillFormButton,
      accessibilityLabel);
  if (IsConditionalPasskeyLoginEnabled()) {
    self.autofillFormButton.accessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_MANUAL_FALLBACK_SIGN_IN_BUTTON_ACCESSIBILITY_LABEL,
        base::SysNSStringToUTF16(accessibilityLabel));
  }
  self.siteNameLabel.hidden = NO;
  self.faviconContainerView.hidden = NO;
  AddViewToVerticalLeadViews(self.headerView,
                             ManualFillCellView::ElementType::kOther,
                             verticalLeadViews);
  AddViewToVerticalLeadViews(self.grayLine,
                             ManualFillCellView::ElementType::kHeaderSeparator,
                             verticalLeadViews);

  if (menuActions && menuActions.count) {
    self.overflowMenuButton.menu = [UIMenu menuWithChildren:menuActions];
    self.overflowMenuButton.hidden = NO;
  } else {
    self.overflowMenuButton.hidden = YES;
  }

  // Holds the chip buttons related to the credential that are vertical leads.
  NSMutableArray<UIView*>* credentialGroupVerticalLeadChips =
      [[NSMutableArray alloc] init];

  // Username chip button.
  if (credential.username.length) {
    NSString* username = credential.username;
    [self.usernameButton setTitle:username forState:UIControlStateNormal];
    self.usernameButton.accessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_MANUAL_FALLBACK_CHIP_ACCESSIBILITY_LABEL,
        base::SysNSStringToUTF16(username));
  } else {
    NSString* titleString =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_USERNAME);
    [self.usernameButton setTitle:titleString forState:UIControlStateNormal];
    [self.usernameButton setTitleColor:[UIColor colorNamed:kTextSecondaryColor]
                              forState:UIControlStateNormal];
    self.usernameButton.enabled = NO;
  }
  [credentialGroupVerticalLeadChips addObject:self.usernameButton];

  // Password chip button.
  if (credentialType == ManualFillCredentialType::kPassword &&
      credential.password.length) {
    [self.passwordButton setTitle:manual_fill::kMaskedPasswordButtonText
                         forState:UIControlStateNormal];
    self.passwordButton.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_MANUAL_FALLBACK_PASSWORD_CHIP_ACCESSIBILITY_LABEL);
    [credentialGroupVerticalLeadChips addObject:self.passwordButton];
    self.passwordButton.hidden = NO;
  } else {
    self.passwordButton.hidden = YES;
  }

  AddChipGroupsToVerticalLeadViews(@[ credentialGroupVerticalLeadChips ],
                                   verticalLeadViews);

  if (showAutofillFormButton) {
    AddViewToVerticalLeadViews(self.autofillFormButton,
                               ManualFillCellView::ElementType::kOther,
                               verticalLeadViews);
    self.autofillFormButton.hidden = NO;
  } else {
    self.autofillFormButton.hidden = YES;
  }

  // Set and activate constraints.
  self.dynamicConstraints = [[NSMutableArray alloc] init];
  AppendVerticalConstraintsSpacingForViews(self.dynamicConstraints,
                                           verticalLeadViews, self.layoutGuide);
  [NSLayoutConstraint activateConstraints:self.dynamicConstraints];
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.credential.URL.spec());
}

- (BOOL)isBackupCredential {
  return self.credential.isBackupCredential;
}

- (void)configureWithFaviconAttributes:(FaviconAttributes*)attributes {
  [self setUpFaviconViewWithAttributes:attributes];
}

- (void)configureWithSymbol:(UIImage*)symbol {
  if (!symbol) {
    return;
  }

  UIImageView* symbolImageView = [[UIImageView alloc] initWithImage:symbol];
  symbolImageView.translatesAutoresizingMaskIntoConstraints = NO;
  symbolImageView.backgroundColor = self.backgroundColor;

  [self.faviconContainerView setFaviconBackgroundColor:self.backgroundColor];
  self.faviconContainerView.hidden = NO;
  [self.faviconContainerView addSubview:symbolImageView];

  [NSLayoutConstraint activateConstraints:@[
    [symbolImageView.centerXAnchor
        constraintEqualToAnchor:self.faviconContainerView.centerXAnchor],
    [symbolImageView.centerYAnchor
        constraintEqualToAnchor:self.faviconContainerView.centerYAnchor],
  ]];

  _customSymbolImageView = symbolImageView;
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  // Holds the views that should be accessible. The ordering in which views are
  // added to this array will reflect the order followed by VoiceOver. Subviews
  // that need to be read by VoiceOver must be added to this array.
  NSMutableArray<UIView*>* accessibilityElements =
      [[NSMutableArray alloc] initWithObjects:self.contentView, nil];

  self.layoutGuide =
      AddLayoutGuideToContentView(self.contentView, /*cell_has_header=*/YES);

  self.selectionStyle = UITableViewCellSelectionStyleNone;

  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];

  self.faviconContainerView = [[FaviconContainerView alloc] init];
  self.faviconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  self.faviconContainerView.clipsToBounds = YES;
  [NSLayoutConstraint activateConstraints:@[
    [self.faviconContainerView.widthAnchor
        constraintEqualToConstant:kFaviconContainterViewSize],
    [self.faviconContainerView.heightAnchor
        constraintEqualToAnchor:self.faviconContainerView.widthAnchor],
  ]];

  self.siteNameLabel = CreateLabel();
  self.overflowMenuButton = CreateOverflowMenuButton(_cellIndex);
  self.headerView = CreateHeaderView(
      self.faviconContainerView, self.siteNameLabel, self.overflowMenuButton);
  [self.contentView addSubview:self.headerView];
  [accessibilityElements addObject:self.overflowMenuButton];
  AppendHorizontalConstraintsForViews(staticConstraints, @[ self.headerView ],
                                      self.layoutGuide);

  self.grayLine = CreateGraySeparatorForContainer(self.contentView);

  self.usernameButton = CreateChipWithSelectorAndTarget(
      @selector(userDidTapUsernameButton:), self);
  [self.contentView addSubview:self.usernameButton];
  [accessibilityElements addObject:self.usernameButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.usernameButton ], self.layoutGuide,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.passwordButton = CreateChipWithSelectorAndTarget(
      @selector(userDidTapPasswordButton:), self);
  [self.contentView addSubview:self.passwordButton];
  [accessibilityElements addObject:self.passwordButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.passwordButton ], self.layoutGuide,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  NSString* buttonTitle = l10n_util::GetNSString(
      IsConditionalPasskeyLoginEnabled()
          ? IDS_IOS_MANUAL_FALLBACK_SIGN_IN_BUTTON_TITLE
          : IDS_IOS_MANUAL_FALLBACK_AUTOFILL_FORM_BUTTON_TITLE);
  self.autofillFormButton = CreateAutofillFormButton(buttonTitle);
  [self.contentView addSubview:self.autofillFormButton];
  [self.autofillFormButton addTarget:self
                              action:@selector(onAutofillFormButtonTapped)
                    forControlEvents:UIControlEventTouchUpInside];
  [accessibilityElements addObject:self.autofillFormButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.autofillFormButton ], self.layoutGuide);

  [NSLayoutConstraint activateConstraints:staticConstraints];

  SetUpCellAccessibilityElements(self, accessibilityElements);
}

- (void)userDidTapUsernameButton:(UIButton*)button {
  [self.mutator didTapUsernameChipForCredential:self.credential];
}

- (void)userDidTapPasswordButton:(UIButton*)button {
  [self.mutator didTapPasswordChipForCredential:self.credential];
}

// Called when the "Autofill Form" button is tapped.
- (void)onAutofillFormButtonTapped {
  [self.mutator didTapAutofillFormButtonForCredential:self.credential
                                            cellIndex:_cellIndex];
}

// Sets up the favicon with the given `attributes`.
- (void)setUpFaviconViewWithAttributes:(FaviconAttributes*)attributes {
  self.faviconContainerView.faviconView.accessibilityIdentifier =
      manual_fill::kExpandedManualFillPasswordFaviconID;
  [self.faviconContainerView.faviconView configureWithAttributes:attributes];
}

// Returns the title for the cell.
- (NSString*)cellTitle {
  if (self.credential.isBackupCredential) {
    return l10n_util::GetNSString(
        IDS_IOS_MANUAL_FALLBACK_RECOVERY_PASSWORD_SUGGESTION_TITLE);
  }
  return self.credential.siteName ?: @"";
}

// Returns the subtitle for the cell.
- (NSString*)cellSubtitle {
  if (self.credential.isBackupCredential) {
    return self.credential.host;
  }
  NSString* credentialSubtext = nil;
  if (IsConditionalPasskeyLoginEnabled()) {
    if (_credentialType == ManualFillCredentialType::kPasskey) {
      credentialSubtext = webauthn::FormatPasskeyManualFillSubtitle(
          self.credential.displayName);
    } else {
      credentialSubtext = l10n_util::GetNSString(IDS_IOS_PASSWORD_SUBTEXT);
    }
  }
  return CreateCredentialSubtitle(self.credential.host,
                                  self.credential.siteName, credentialSubtext);
}

@end
