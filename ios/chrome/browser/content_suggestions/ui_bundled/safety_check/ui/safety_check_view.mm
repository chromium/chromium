// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/ui/safety_check_view.h"

#import "base/check.h"
#import "base/strings/string_number_conversions.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_detail_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_detail_view_configuration.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/icon_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/cells/multi_row_container_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_content_view_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/model/safety_check_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/public/safety_check_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/ui/safety_check_audience.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/ui/safety_check_item_type.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/safety_check/ui/safety_check_state.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Returns `true` if any of the safety check components are currently running.
bool IsRunning(SafetyCheckState* state) {
  return state.runningState == RunningSafetyCheckState::kRunning ||
         state.updateChromeState == UpdateChromeSafetyCheckState::kRunning ||
         state.passwordState == PasswordSafetyCheckState::kRunning ||
         state.safeBrowsingState == SafeBrowsingSafetyCheckState::kRunning;
}

// Returns `true` if all of the safety check components are in the default
// state.
bool IsDefault(SafetyCheckState* state) {
  return state.runningState == RunningSafetyCheckState::kDefault &&
         state.updateChromeState == UpdateChromeSafetyCheckState::kDefault &&
         state.passwordState == PasswordSafetyCheckState::kDefault &&
         state.safeBrowsingState == SafeBrowsingSafetyCheckState::kDefault;
}

}  // namespace

@interface SafetyCheckView () <IconDetailViewTapDelegate>
@end

@implementation SafetyCheckView {
  id<MagicStackModuleContentViewDelegate> _contentViewDelegate;
  SafetyCheckState* _state;
  UIView* _contentView;

  // The background color applied behind the symbol.
  UIColor* _symbolBackgroundColor;

  // The background color of the container if the symbol is rendered in a
  // container.
  UIColor* _symbolContainerBackgroundColor;

  // The array of foreground colors used to render the symbol.
  NSArray<UIColor*>* _symbolColorPalette;
}

#pragma mark - Public methods

- (instancetype)initWithState:(SafetyCheckState*)state
          contentViewDelegate:
              (id<MagicStackModuleContentViewDelegate>)contentViewDelegate {
  if ((self = [super init])) {
    _contentViewDelegate = contentViewDelegate;
    _state = state;

    if (IsNTPBackgroundCustomizationEnabled()) {
      [self registerForTraitChanges:@[ NewTabPageTrait.class ]
                         withAction:@selector(applyBackgroundColors)];
    }
    [self applyBackgroundColors];
  }

  return self;
}

#pragma mark - SafetyCheckMagicStackConsumer

- (void)safetyCheckStateDidChange:(SafetyCheckState*)state {
  _state = state;
  if (_contentView) {
    [_contentView removeFromSuperview];
  }
  [self createSubviews];
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - IconDetailViewTapDelegate

- (void)didTapIconDetailView:(IconDetailView*)view {
  CHECK(view.identifier != nil);

  SafetyCheckItemType itemType = SafetyCheckItemTypeForName(view.identifier);

  [self.audience didSelectSafetyCheckItem:itemType];
}

#pragma mark - NewTabPageColorUpdating

- (void)applyBackgroundColors {
  NewTabPageColorPalette* colorPalette =
      IsNTPBackgroundCustomizationEnabled()
          ? [self.traitCollection objectForNewTabPageTrait]
          : nil;

  int checkIssuesCount = [_state numberOfIssues];
  BOOL isRunning = IsRunning(_state);
  BOOL isDefault = IsDefault(_state);
  BOOL isHeroLayout = isRunning || isDefault || checkIssuesCount <= 1;

  // Determine the symbol color palette and symbol background color based on
  // the layout.
  if (isHeroLayout) {
    _symbolColorPalette = @[ [UIColor whiteColor] ];
    _symbolBackgroundColor = [UIColor colorNamed:kBlue500Color];
    _symbolContainerBackgroundColor = colorPalette
                                          ? colorPalette.tertiaryColor
                                          : [UIColor colorNamed:kGrey100Color];
  } else if (colorPalette) {
    _symbolColorPalette = @[ colorPalette.tintColor ];
    _symbolBackgroundColor = colorPalette.tertiaryColor;
    _symbolContainerBackgroundColor = colorPalette.tertiaryColor;
  } else {
    _symbolColorPalette = @[ [UIColor colorNamed:kBlue500Color] ];
    _symbolBackgroundColor = [UIColor colorNamed:kBlueHaloColor];
    _symbolContainerBackgroundColor = [UIColor colorNamed:kGrey100Color];
  }

  // Redraws the view by removing and recreating the content view.
  [self safetyCheckStateDidChange:_state];
}

#pragma mark - Private methods

// Creates all views for the Safety Check (Magic Stack) module.
- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = safety_check::kSafetyCheckViewID;

  [_contentViewDelegate
      setSubtitle:FormatElapsedTimeSinceLastSafetyCheck(_state.lastRunTime)];

  [_contentViewDelegate
      updateNotificationsOptInVisibility:_state.showNotificationsOptIn];

  int checkIssuesCount = [_state numberOfIssues];

  // Determine whether the separator should be hidden.
  BOOL hideSeparator = checkIssuesCount > 1;
  [_contentViewDelegate updateSeparatorVisibility:hideSeparator];

  BOOL isRunning = IsRunning(_state);
  BOOL isDefault = IsDefault(_state);
  SafetyCheckItemType itemType;

  if (isRunning) {
    itemType = SafetyCheckItemType::kRunning;
  } else if (isDefault) {
    itemType = SafetyCheckItemType::kDefault;
  } else if (checkIssuesCount == 0) {
    itemType = SafetyCheckItemType::kAllSafe;
  } else if (checkIssuesCount == 1) {
    if (InvalidUpdateChromeState(_state.updateChromeState)) {
      itemType = SafetyCheckItemType::kUpdateChrome;
    } else if (InvalidPasswordState(_state.passwordState)) {
      itemType = SafetyCheckItemType::kPassword;
    } else if (InvalidSafeBrowsingState(_state.safeBrowsingState)) {
      itemType = SafetyCheckItemType::kSafeBrowsing;
    } else {
      NOTREACHED();
    }
  }

  if (isRunning || isDefault || checkIssuesCount <= 1) {
    _contentView = [self iconDetailView:itemType
                             layoutType:IconDetailViewLayoutType::kHero];
    [self addSubview:_contentView];
    AddSameConstraints(_contentView, self);
    return;
  }

  NSMutableArray<IconDetailView*>* safetyCheckItems =
      [[NSMutableArray alloc] init];

  if (InvalidUpdateChromeState(_state.updateChromeState)) {
    [safetyCheckItems
        addObject:[self iconDetailView:SafetyCheckItemType::kUpdateChrome
                            layoutType:IconDetailViewLayoutType::kCompact]];
  }

  if (InvalidPasswordState(_state.passwordState)) {
    [safetyCheckItems
        addObject:[self iconDetailView:SafetyCheckItemType::kPassword
                            layoutType:IconDetailViewLayoutType::kCompact]];
  }

  // NOTE: Don't add the Safe Browsing check if two items already exist in
  // `safetyCheckItems`. At most, the compact view displays two rows of items.
  if ([safetyCheckItems count] < 2 &&
      InvalidSafeBrowsingState(_state.safeBrowsingState)) {
    [safetyCheckItems
        addObject:[self iconDetailView:SafetyCheckItemType::kSafeBrowsing
                            layoutType:IconDetailViewLayoutType::kCompact]];
  }

  _contentView = [[MultiRowContainerView alloc] initWithViews:safetyCheckItems];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_contentView];
  AddSameConstraints(_contentView, self);
}

// Creates and returns an `IconDetailView` configured for the given `itemType`
// and `layoutType`.
- (IconDetailView*)iconDetailView:(SafetyCheckItemType)itemType
                       layoutType:(IconDetailViewLayoutType)layoutType {

  if (!IsNTPBackgroundCustomizationEnabled()) {
    // Determine the symbol color palette and symbol background color based on
    // the layout.
    _symbolColorPalette = @[ [UIColor whiteColor] ];
    _symbolBackgroundColor = [UIColor colorNamed:kBlue500Color];

    // Compact, in-square icons are displayed in blue with a light blue
    // container.
    if (layoutType == IconDetailViewLayoutType::kCompact) {
      _symbolColorPalette = @[ [UIColor colorNamed:kBlue500Color] ];
      _symbolBackgroundColor = [UIColor colorNamed:kBlueHaloColor];
    }
  }

  NSString* symbolName = [self symbolNameForItemType:itemType];

  IconDetailViewConfiguration* viewConfig = [IconDetailViewConfiguration
      configurationWithTitleText:[self titleText:itemType]
                 descriptionText:(layoutType == IconDetailViewLayoutType::kHero
                                      ? [self descriptionText:itemType]
                                      : [self
                                            compactDescriptionText:itemType])];
  viewConfig.symbolName = symbolName;
  viewConfig.symbolColorPalette = _symbolColorPalette;
  viewConfig.symbolBackgroundColor = _symbolBackgroundColor;
  viewConfig.usesDefaultSymbol = [symbolName isEqualToString:kInfoCircleSymbol];
  viewConfig.layoutType = layoutType;
  viewConfig.showCheckmark = itemType == SafetyCheckItemType::kAllSafe;
  viewConfig.accessibilityIdentifier =
      [self accessibilityIdentifierForItemType:itemType];

  IconDetailView* view =
      [[IconDetailView alloc] initWithConfiguration:viewConfig];
  view.identifier = NameForSafetyCheckItemType(itemType);
  view.tapDelegate = self;
  return view;
}

// Returns the title text for the given `itemType`.
- (NSString*)titleText:(SafetyCheckItemType)itemType {
  switch (itemType) {
    case SafetyCheckItemType::kAllSafe:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_ALL_SAFE);
    case SafetyCheckItemType::kRunning:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_RUNNING);
    case SafetyCheckItemType::kUpdateChrome:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_UPDATE_CHROME);
    case SafetyCheckItemType::kPassword:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_PASSWORD);
    case SafetyCheckItemType::kSafeBrowsing:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_SAFE_BROWSING);
    case SafetyCheckItemType::kDefault:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_DEFAULT);
  }
}

// Returns the detailed description text for the given `itemType`.
- (NSString*)descriptionText:(SafetyCheckItemType)itemType {
  switch (itemType) {
    case SafetyCheckItemType::kAllSafe:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_ALL_SAFE);
    case SafetyCheckItemType::kRunning:
      // The running state has no description text.
      return @"";
    case SafetyCheckItemType::kUpdateChrome:
      return [self updateChromeItemDescriptionText];
    case SafetyCheckItemType::kPassword:
      return [self passwordItemDescriptionText];
    case SafetyCheckItemType::kSafeBrowsing:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_DESCRIPTION_SAFE_BROWSING);
    case SafetyCheckItemType::kDefault:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_DEFAULT);
  }
}

// Returns the compact description text for the given `itemType`.
- (NSString*)compactDescriptionText:(SafetyCheckItemType)itemType {
  switch (itemType) {
    case SafetyCheckItemType::kAllSafe:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_ALL_SAFE);
    case SafetyCheckItemType::kRunning:
      // The running state has no description text.
      return @"";
    case SafetyCheckItemType::kUpdateChrome:
      return [self updateChromeItemCompactDescriptionText];
    case SafetyCheckItemType::kPassword:
      return [self passwordItemCompactDescriptionText];
    case SafetyCheckItemType::kSafeBrowsing:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_SAFE_BROWSING);
    case SafetyCheckItemType::kDefault:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_DEFAULT);
  }
}

// Returns the detailed description text for the Update Chrome item, considering
// the current channel.
- (NSString*)updateChromeItemDescriptionText {
  switch (::GetChannel()) {
    case version_info::Channel::STABLE:
    case version_info::Channel::DEV:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_DESCRIPTION_UPDATE_CHROME);
    case version_info::Channel::BETA:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_BETA_DESC);
    case version_info::Channel::CANARY:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_CANARY_DESC);
    default:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_DESCRIPTION_UPDATE_CHROME);
  }
}

// Returns the compact description text for the Update Chrome item, considering
// the current channel.
- (NSString*)updateChromeItemCompactDescriptionText {
  switch (::GetChannel()) {
    case version_info::Channel::STABLE:
    case version_info::Channel::DEV:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_UPDATE_CHROME);
    case version_info::Channel::BETA:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_BETA_DESC);
    case version_info::Channel::CANARY:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_CANARY_DESC);
    default:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_UPDATE_CHROME);
  }
}

// Returns the detailed description text for the Password item, based on the
// number of compromised, reused, and weak passwords.
- (NSString*)passwordItemDescriptionText {
  if (_state.compromisedPasswordsCount > 1) {
    return l10n_util::GetNSStringF(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_MULTIPLE_COMPROMISED_PASSWORDS,
        base::NumberToString16(_state.compromisedPasswordsCount));
  }

  if (_state.compromisedPasswordsCount == 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_COMPROMISED_PASSWORD);
  }

  if (_state.reusedPasswordsCount > 1) {
    return l10n_util::GetNSStringF(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_MULTIPLE_REUSED_PASSWORDS,
        base::NumberToString16(_state.reusedPasswordsCount));
  }

  if (_state.reusedPasswordsCount == 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_REUSED_PASSWORD);
  }

  if (_state.weakPasswordsCount > 1) {
    return l10n_util::GetNSStringF(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_MULTIPLE_WEAK_PASSWORDS,
        base::NumberToString16(_state.weakPasswordsCount));
  }

  if (_state.weakPasswordsCount == 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_WEAK_PASSWORD);
  }

  return l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_MULTIPLE_PASSWORD_ISSUES);
}

// Returns the compact description text for the Password item, based on the
// presence of compromised, reused, or weak passwords.
- (NSString*)passwordItemCompactDescriptionText {
  if (_state.compromisedPasswordsCount >= 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_COMPROMISED_PASSWORD);
  }

  if (_state.reusedPasswordsCount >= 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_REUSED_PASSWORD);
  }

  if (_state.weakPasswordsCount >= 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_WEAK_PASSWORD);
  }

  return l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_MULTIPLE_PASSWORD_ISSUES);
}

// Returns the symbol name for the given `itemType`.
- (NSString*)symbolNameForItemType:(SafetyCheckItemType)itemType {
  switch (itemType) {
    case SafetyCheckItemType::kUpdateChrome:
      return kInfoCircleSymbol;
    case SafetyCheckItemType::kPassword:
      return kPasswordSymbol;
    case SafetyCheckItemType::kSafeBrowsing:
      return kPrivacySymbol;
    case SafetyCheckItemType::kAllSafe:
    case SafetyCheckItemType::kRunning:
    case SafetyCheckItemType::kDefault:
      return kSafetyCheckSymbol;
  }
}

// Returns the accessibility identifier for the given `itemType`.
- (NSString*)accessibilityIdentifierForItemType:(SafetyCheckItemType)itemType {
  switch (itemType) {
    case SafetyCheckItemType::kAllSafe:
      return safety_check::kAllSafeItemID;
    case SafetyCheckItemType::kRunning:
      return safety_check::kRunningItemID;
    case SafetyCheckItemType::kUpdateChrome:
      return safety_check::kUpdateChromeItemID;
    case SafetyCheckItemType::kPassword:
      return safety_check::kPasswordItemID;
    case SafetyCheckItemType::kSafeBrowsing:
      return safety_check::kSafeBrowsingItemID;
    case SafetyCheckItemType::kDefault:
      return safety_check::kDefaultItemID;
  }
}

@end
