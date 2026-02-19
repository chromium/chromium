// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_view.h"

#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_content_view_delegate.h"
#import "ios/chrome/browser/content_suggestions/safety_check/model/safety_check_utils.h"
#import "ios/chrome/browser/content_suggestions/safety_check/public/safety_check_constants.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_config.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_item_type.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/multi_row_container_view.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation SafetyCheckView {
  // The configuration of a Safety Check item's view.
  SafetyCheckConfig* _config;

  // Content view for Safety Check.
  UIView* _contentView;

  // Delegate for the `_contentView`.
  id<MagicStackModuleContentViewDelegate> _contentViewDelegate;
}

#pragma mark - Public methods

- (instancetype)initWithConfig:(SafetyCheckConfig*)config
           contentViewDelegate:
               (id<MagicStackModuleContentViewDelegate>)contentViewDelegate {
  if ((self = [super init])) {
    _contentViewDelegate = contentViewDelegate;
    _config = config;
  }

  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];
  [self createSubviews];
}

#pragma mark - Private

// Creates all views for the Safety Check (Magic Stack) module.
- (void)createSubviews {
  if (_contentView) {
    [_contentView removeFromSuperview];
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = safety_check::kSafetyCheckViewID;

  [_contentViewDelegate
      setSubtitle:FormatElapsedTimeSinceLastSafetyCheck(_config.lastRunTime)];

  [_contentViewDelegate
      updateNotificationsOptInVisibility:_config.showNotificationsOptIn];

  NSUInteger checkIssuesCount = [_config numberOfIssues];

  // Determine whether the separator should be hidden.
  BOOL hideSeparator = checkIssuesCount > 1;
  [_contentViewDelegate updateSeparatorVisibility:hideSeparator];

  SafetyCheckItemType itemType;

  if ([_config isRunning]) {
    itemType = SafetyCheckItemType::kRunning;
  } else if ([_config isDefault]) {
    itemType = SafetyCheckItemType::kDefault;
  } else if (checkIssuesCount == 0) {
    itemType = SafetyCheckItemType::kAllSafe;
  } else if (checkIssuesCount == 1) {
    if (InvalidUpdateChromeState(_config.updateChromeState)) {
      itemType = SafetyCheckItemType::kUpdateChrome;
    } else if (InvalidPasswordState(_config.passwordState)) {
      itemType = SafetyCheckItemType::kPassword;
    } else if (InvalidSafeBrowsingState(_config.safeBrowsingState)) {
      itemType = SafetyCheckItemType::kSafeBrowsing;
    } else {
      NOTREACHED();
    }
  }

  if (_config.layoutType == IconDetailViewLayoutType::kHero) {
    // Build view with single-item Hero layout.
    _contentView = [self createIconDetailView:itemType];
    [self addSubview:_contentView];
    AddSameConstraints(_contentView, self);
    return;
  }

  // Build view with multi-row compact layout.
  NSMutableArray<IconDetailView*>* safetyCheckItems =
      [[NSMutableArray alloc] init];

  if (InvalidUpdateChromeState(_config.updateChromeState)) {
    [safetyCheckItems
        addObject:[self
                      createIconDetailView:SafetyCheckItemType::kUpdateChrome]];
  }

  if (InvalidPasswordState(_config.passwordState)) {
    [safetyCheckItems
        addObject:[self createIconDetailView:SafetyCheckItemType::kPassword]];
  }

  // NOTE: Don't add the Safe Browsing check if two items already exist in
  // `safetyCheckItems`. At most, the compact view displays two rows of items.
  if ([safetyCheckItems count] < 2 &&
      InvalidSafeBrowsingState(_config.safeBrowsingState)) {
    [safetyCheckItems
        addObject:[self
                      createIconDetailView:SafetyCheckItemType::kSafeBrowsing]];
  }

  _contentView = [[MultiRowContainerView alloc] initWithViews:safetyCheckItems];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_contentView];
  AddSameConstraints(_contentView, self);
}

// Creates and returns an `IconDetailView` configured for the given `itemType`.
- (IconDetailView*)createIconDetailView:(SafetyCheckItemType)itemType {
  _config.itemType = itemType;
  IconDetailView* view = [[IconDetailView alloc] initWithConfig:_config];
  view.identifier = NameForSafetyCheckItemType(_config.itemType);
  view.tapDelegate = _config;
  return view;
}

@end
