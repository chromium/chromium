// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_view.h"

#import "base/check.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module_content_view_delegate.h"
#import "ios/chrome/browser/content_suggestions/safety_check/model/safety_check_utils.h"
#import "ios/chrome/browser/content_suggestions/safety_check/public/safety_check_constants.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_item_type.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_state.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/icon_detail_view.h"
#import "ios/chrome/browser/content_suggestions/ui/cells/multi_row_container_view.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation SafetyCheckView {
  // The configuration of a Safety Check item's view.
  SafetyCheckState* _state;

  // Content view for Safety Check.
  UIView* _contentView;

  // Delegate for the `_contentView`.
  id<MagicStackModuleContentViewDelegate> _contentViewDelegate;
}

#pragma mark - Public methods

- (instancetype)initWithState:(SafetyCheckState*)state
          contentViewDelegate:
              (id<MagicStackModuleContentViewDelegate>)contentViewDelegate {
  if ((self = [super init])) {
    _contentViewDelegate = contentViewDelegate;
    _state = state;
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
      setSubtitle:FormatElapsedTimeSinceLastSafetyCheck(_state.lastRunTime)];

  [_contentViewDelegate
      updateNotificationsOptInVisibility:_state.showNotificationsOptIn];

  NSUInteger checkIssuesCount = [_state numberOfIssues];

  // Determine whether the separator should be hidden.
  BOOL hideSeparator = checkIssuesCount > 1;
  [_contentViewDelegate updateSeparatorVisibility:hideSeparator];

  SafetyCheckItemType itemType;

  if ([_state isRunning]) {
    itemType = SafetyCheckItemType::kRunning;
  } else if ([_state isDefault]) {
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

  if (_state.layoutType == IconDetailViewLayoutType::kHero) {
    // Build view with single-item Hero layout.
    _contentView = [self createIconDetailView:itemType];
    [self addSubview:_contentView];
    AddSameConstraints(_contentView, self);
    return;
  }

  // Build view with multi-row compact layout.
  NSMutableArray<IconDetailView*>* safetyCheckItems =
      [[NSMutableArray alloc] init];

  if (InvalidUpdateChromeState(_state.updateChromeState)) {
    [safetyCheckItems
        addObject:[self
                      createIconDetailView:SafetyCheckItemType::kUpdateChrome]];
  }

  if (InvalidPasswordState(_state.passwordState)) {
    [safetyCheckItems
        addObject:[self createIconDetailView:SafetyCheckItemType::kPassword]];
  }

  // NOTE: Don't add the Safe Browsing check if two items already exist in
  // `safetyCheckItems`. At most, the compact view displays two rows of items.
  if ([safetyCheckItems count] < 2 &&
      InvalidSafeBrowsingState(_state.safeBrowsingState)) {
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
  _state.itemType = itemType;
  IconDetailView* view = [[IconDetailView alloc] initWithConfig:_state];
  view.identifier = NameForSafetyCheckItemType(_state.itemType);
  view.tapDelegate = _state;
  return view;
}

@end
