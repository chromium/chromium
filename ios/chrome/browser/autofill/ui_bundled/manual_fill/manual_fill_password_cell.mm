// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_cell.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

namespace {

constexpr CGFloat kFaviconContainterViewSize = 30;

// Returns the size that the favicon should have.
CGFloat GetFaviconSize() {
  return IsKeyboardAccessoryUpgradeEnabled() ? kFaviconContainterViewSize
                                             : gfx::kFaviconSize;
}

}  // namespace

@interface ManualFillCredentialItem ()

// The credential for this item.
@property(nonatomic, strong, readonly) ManualFillCredential* credential;

// The cell won't show a title (site name) label if it is connected to the
// previous password item.
// TODO(crbug.com/326398845): Remove once the Keyboard Accessory Upgrade feature
// has launched both on iPhone and iPad.
@property(nonatomic, assign) BOOL isConnectedToPreviousItem;

// The separator line won't show if it is connected to the next password item.
// TODO(crbug.com/326398845): Remove once the Keyboard Accessory Upgrade feature
// has launched both on iPhone and iPad.
@property(nonatomic, assign) BOOL isConnectedToNextItem;

// The delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentInjector>
    contentInjector;

// The UIActions that should be available from the cell's overflow menu button.
@property(nonatomic, strong) NSArray<UIAction*>* menuActions;

// The part of the cell's accessibility label that is used to indicate the index
// at which the password represented by this item is positioned in the list of
// passwords to show.
@property(nonatomic, strong) NSString* cellIndexAccessibilityLabel;

@end

@implementation ManualFillCredentialItem {
  // The 0-based index at which the password is in the list of passwords to
  // show.
  NSInteger _cellIndex;

  // If `YES`, autofill button is shown for the item.
  BOOL _showAutofillFormButton;

  // If `YES`, the cell represented by this item is displayed in the all
  // password list.
  BOOL _fromAllPasswordsContext;
}

- (instancetype)initWithCredential:(ManualFillCredential*)credential
         isConnectedToPreviousItem:(BOOL)isConnectedToPreviousItem
             isConnectedToNextItem:(BOOL)isConnectedToNextItem
                   contentInjector:
                       (id<ManualFillContentInjector>)contentInjector
                       menuActions:(NSArray<UIAction*>*)menuActions
                         cellIndex:(NSInteger)cellIndex
       cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
            showAutofillFormButton:(BOOL)showAutofillFormButton
           fromAllPasswordsContext:(BOOL)fromAllPasswordsContext {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _credential = credential;
    _isConnectedToPreviousItem = isConnectedToPreviousItem;
    _isConnectedToNextItem = isConnectedToNextItem;
    _contentInjector = contentInjector;
    _menuActions = menuActions;
    _cellIndex = cellIndex;
    _cellIndexAccessibilityLabel = cellIndexAccessibilityLabel;
    _showAutofillFormButton = showAutofillFormButton;
    _fromAllPasswordsContext = fromAllPasswordsContext;
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
                  contentInjector:self.contentInjector
                      menuActions:self.menuActions
                        cellIndex:_cellIndex
      cellIndexAccessibilityLabel:_cellIndexAccessibilityLabel
           showAutofillFormButton:_showAutofillFormButton
          fromAllPasswordsContext:_fromAllPasswordsContext];
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

// The offset to apply to a cell's top margin when connected to the previous
// cell.
static const CGFloat kOffsetForConnectedCell = 16;

}  // namespace

@interface ManualFillPasswordCell ()

// The credential this cell is showing.
@property(nonatomic, strong) ManualFillCredential* credential;

// The dynamic constraints for all the lines (i.e. not set in createView).
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* dynamicConstraints;

// The constraints for the visible favicon.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* faviconContraints;

// The view displayed at the top the cell containing the favicon, the site name
// and an overflow button.
@property(nonatomic, strong) UIView* headerView;

// The favicon for the credential. Of type FaviconView when the Keyboard
// Accessory Upgrade is disabled, and FaviconContainerView when enabled.
@property(nonatomic, strong) UIView* faviconView;

// The label with the site name and host.
@property(nonatomic, strong) UILabel* siteNameLabel;

// The menu button displayed in the cell's header.
@property(nonatomic, strong) UIButton* overflowMenuButton;

// A button showing the username, or "No Username".
@property(nonatomic, strong) UIButton* usernameButton;

// A button showing "••••••••" to resemble a password.
@property(nonatomic, strong) UIButton* passwordButton;

// Separator line. When the Keyboard Accessory Upgrade feature is enbaled, used
// to delimit the header from the rest of the cell. When disabled, used when
// needed to delimit cells.
@property(nonatomic, strong) UIView* grayLine;

// The delegate in charge of processing the user actions in this cell.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;

// Layout guide for the cell's content.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

// Button to autofill the current form with the credential's data.
@property(nonatomic, strong) UIButton* autofillFormButton;

@end

@implementation ManualFillPasswordCell {
  // The 0-based index at which the password is in the list of passwords to
  // show.
  NSInteger _cellIndex;

  // If `YES`, the cell is displayed in the all password list.
  BOOL _fromAllPasswordsContext;
}

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];
  [NSLayoutConstraint deactivateConstraints:self.faviconContraints];
  self.faviconView.hidden = YES;

  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];

  self.siteNameLabel.text = @"";
  [self configureFaviconWithAttributes:nil];

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
                contentInjector:(id<ManualFillContentInjector>)contentInjector
                    menuActions:(NSArray<UIAction*>*)menuActions
                      cellIndex:(NSInteger)cellIndex
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
         showAutofillFormButton:(BOOL)showAutofillFormButton
        fromAllPasswordsContext:(BOOL)fromAllPasswordsContext {
  _cellIndex = cellIndex;
  _fromAllPasswordsContext = fromAllPasswordsContext;

  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.contentInjector = contentInjector;
  self.credential = credential;

  // Holds the views whose leading anchor is constrained relative to the cell's
  // leading anchor.
  std::vector<ManualFillCellView> verticalLeadViews;

  // Header.
  if (isConnectedToPreviousCell) {
    self.siteNameLabel.hidden = YES;
    self.faviconView.hidden = YES;
  } else {
    BOOL shouldShowHost =
        credential.host && credential.host.length &&
        ![credential.host isEqualToString:credential.siteName];

    NSAttributedString* attributedText =
        CreateSiteNameLabelAttributedText(credential, shouldShowHost);
    self.siteNameLabel.attributedText = attributedText;
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      self.siteNameLabel.numberOfLines = 0;
      NSString* accessibilityLabel =
          [NSString stringWithFormat:@"%@, %@", cellIndexAccessibilityLabel,
                                     attributedText.string];
      GiveAccessibilityContextToCellAndButton(
          self.contentView, self.overflowMenuButton, self.autofillFormButton,
          accessibilityLabel);
    }
    self.siteNameLabel.hidden = NO;
    self.faviconView.hidden = NO;
    AddViewToVerticalLeadViews(self.headerView,
                               ManualFillCellView::ElementType::kOther,
                               verticalLeadViews);
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      AddViewToVerticalLeadViews(
          self.grayLine, ManualFillCellView::ElementType::kHeaderSeparator,
          verticalLeadViews);
    }
  }
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
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      self.usernameButton.accessibilityLabel = l10n_util::GetNSStringF(
          IDS_IOS_MANUAL_FALLBACK_CHIP_ACCESSIBILITY_LABEL,
          base::SysNSStringToUTF16(username));
    }
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
  if (credential.password.length) {
    [self.passwordButton setTitle:manual_fill::kMaskedPasswordButtonText
                         forState:UIControlStateNormal];
    self.passwordButton.accessibilityLabel = l10n_util::GetNSString(
        IsKeyboardAccessoryUpgradeEnabled()
            ? IDS_IOS_MANUAL_FALLBACK_PASSWORD_CHIP_ACCESSIBILITY_LABEL
            : IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL);
    [credentialGroupVerticalLeadChips addObject:self.passwordButton];
    self.passwordButton.hidden = NO;
  } else {
    self.passwordButton.hidden = YES;
  }

  AddChipGroupsToVerticalLeadViews(@[ credentialGroupVerticalLeadChips ],
                                   verticalLeadViews);

  if (isConnectedToNextCell) {
    self.grayLine.hidden = YES;
  }

  if (showAutofillFormButton) {
    CHECK(IsKeyboardAccessoryUpgradeEnabled());
    AddViewToVerticalLeadViews(self.autofillFormButton,
                               ManualFillCellView::ElementType::kOther,
                               verticalLeadViews);
    self.autofillFormButton.hidden = NO;
  } else {
    self.autofillFormButton.hidden = YES;
  }

  // Set and activate constraints.
  self.dynamicConstraints = [[NSMutableArray alloc] init];
  CGFloat offset = isConnectedToPreviousCell ? -kOffsetForConnectedCell : 0;
  AppendVerticalConstraintsSpacingForViews(
      self.dynamicConstraints, verticalLeadViews, self.layoutGuide, offset);
  [NSLayoutConstraint activateConstraints:self.dynamicConstraints];
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.credential.URL.spec());
}

- (void)configureWithFaviconAttributes:(FaviconAttributes*)attributes {
  if (attributes.faviconImage) {
    self.faviconView.hidden = NO;
    [NSLayoutConstraint activateConstraints:self.faviconContraints];
    [self configureFaviconWithAttributes:attributes];
    return;
  }
  [NSLayoutConstraint deactivateConstraints:self.faviconContraints];
  self.faviconView.hidden = YES;
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  // Holds the views that should be accessible. The ordering in which views are
  // added to this array will reflect the order followed by VoiceOver. When the
  // Keyboard Accessory Upgrade feature is enabled, subviews that need to be
  // read by VoiceOver must be added to this array. Otherwise, they will be
  // ignored.
  NSMutableArray<UIView*>* accessibilityElements =
      [[NSMutableArray alloc] initWithObjects:self.contentView, nil];

  self.layoutGuide =
      AddLayoutGuideToContentView(self.contentView, /*cell_has_header=*/YES);

  self.selectionStyle = UITableViewCellSelectionStyleNone;

  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];

  self.faviconView = IsKeyboardAccessoryUpgradeEnabled()
                         ? [[FaviconContainerView alloc] init]
                         : [[FaviconView alloc] init];
  self.faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  self.faviconView.clipsToBounds = YES;
  self.faviconView.hidden = YES;
  self.faviconContraints = @[
    [self.faviconView.widthAnchor constraintEqualToConstant:GetFaviconSize()],
    [self.faviconView.heightAnchor
        constraintEqualToAnchor:self.faviconView.widthAnchor],
  ];

  self.siteNameLabel = CreateLabel();
  self.overflowMenuButton = CreateOverflowMenuButton();
  self.headerView = CreateHeaderView(self.faviconView, self.siteNameLabel,
                                     self.overflowMenuButton);
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
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.passwordButton = CreateChipWithSelectorAndTarget(
      @selector(userDidTapPasswordButton:), self);
  [self.contentView addSubview:self.passwordButton];
  [accessibilityElements addObject:self.passwordButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.passwordButton ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

    self.autofillFormButton = CreateAutofillFormButton();
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

// Called when the "Autofill Form" button is tapped. Fills the current form with
// the credential' data.
- (void)onAutofillFormButtonTapped {
  std::string histogram =
      "Autofill.UserAcceptedSuggestionAtIndex.Password.ManualFallback";
  if (_fromAllPasswordsContext) {
    histogram = base::StrCat({histogram, ".AllPasswords"});
    base::RecordAction(base::UserMetricsAction(
        "ManualFallback_AllPasswords_SuggestionAccepted"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("ManualFallback_Password_SuggestionAccepted"));
  }
  base::UmaHistogramSparse(histogram, _cellIndex);

  [self.contentInjector autofillFormWithCredential:self.credential
                                      shouldReauth:!_fromAllPasswordsContext];
}

// Configure the favicon with the given `attributes`.
- (void)configureFaviconWithAttributes:(FaviconAttributes*)attributes {
  FaviconView* favicon;
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    FaviconContainerView* faviconContainerView =
        static_cast<FaviconContainerView*>(self.faviconView);
    favicon = faviconContainerView.faviconView;
  } else {
    favicon = static_cast<FaviconView*>(self.faviconView);
  }
  [favicon configureWithAttributes:attributes];
}

@end
