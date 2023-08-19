// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_view.h"

#import "ios/chrome/browser/safety_check/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/multi_row_container_view.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_item_view.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/types.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Returns the number of check issue types found given `state`.
int CheckIssuesCount(SafetyCheckState* state) {
  int count = 0;

  if (state.updateChromeState != UpdateChromeSafetyCheckState::kUpToDate) {
    count++;
  }

  if (state.safeBrowsingState != SafeBrowsingSafetyCheckState::kSafe) {
    count++;
  }

  if (state.passwordState != PasswordSafetyCheckState::kSafe) {
    count++;
  }

  return count;
}

}  // namespace

@interface SafetyCheckView () <SafetyCheckItemViewTapDelegate>
@end

@implementation SafetyCheckView {
  SafetyCheckState* _state;
}

#pragma mark - Public methods

- (instancetype)initWithState:(SafetyCheckState*)state {
  if (self = [super init]) {
    _state = state;
  }

  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - SafetyCheckItemViewTapDelegate

- (void)didTapSafetyCheckItemView:(SafetyCheckItemView*)view {
  [self.delegate didSelectSafetyCheckItem:view.itemType];
}

#pragma mark - Private methods

- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = safety_check::kSafetyCheckViewID;

  // If any of the checks are running, the module should display its running
  // state.
  if (_state.runningState == RunningSafetyCheckState::kRunning ||
      _state.updateChromeState == UpdateChromeSafetyCheckState::kRunning ||
      _state.passwordState == PasswordSafetyCheckState::kRunning ||
      _state.safeBrowsingState == SafeBrowsingSafetyCheckState::kRunning) {
    SafetyCheckItemView* view = [[SafetyCheckItemView alloc]
        initWithItemType:SafetyCheckItemType::kRunning
              layoutType:SafetyCheckItemLayoutType::kHero];

    view.tapDelegate = self;

    [self addSubview:view];

    AddSameConstraints(view, self);

    return;
  }

  // If all checks are in the default state, the module should display the
  // default state.
  if (_state.runningState == RunningSafetyCheckState::kDefault &&
      _state.updateChromeState == UpdateChromeSafetyCheckState::kDefault &&
      _state.passwordState == PasswordSafetyCheckState::kDefault &&
      _state.safeBrowsingState == SafeBrowsingSafetyCheckState::kDefault) {
    SafetyCheckItemView* view = [[SafetyCheckItemView alloc]
        initWithItemType:SafetyCheckItemType::kDefault
              layoutType:SafetyCheckItemLayoutType::kHero];

    view.tapDelegate = self;

    [self addSubview:view];

    AddSameConstraints(view, self);

    return;
  }

  int checkIssuesCount = CheckIssuesCount(_state);

  // Show the "All Safe" state if there are no check issues.
  if (checkIssuesCount == 0) {
    SafetyCheckItemView* view = [[SafetyCheckItemView alloc]
        initWithItemType:SafetyCheckItemType::kAllSafe
              layoutType:SafetyCheckItemLayoutType::kHero];

    view.tapDelegate = self;

    [self addSubview:view];

    AddSameConstraints(view, self);

    return;
  }

  if (checkIssuesCount > 1) {
    NSMutableArray<SafetyCheckItemView*>* safetyCheckItems =
        [[NSMutableArray alloc] init];

    // Update Chrome check
    if (_state.updateChromeState != UpdateChromeSafetyCheckState::kUpToDate) {
      SafetyCheckItemView* updateChromeView = [[SafetyCheckItemView alloc]
          initWithItemType:SafetyCheckItemType::kUpdateChrome
                layoutType:SafetyCheckItemLayoutType::kCompact];

      updateChromeView.tapDelegate = self;

      [safetyCheckItems addObject:updateChromeView];
    }

    // Password check
    if (_state.passwordState != PasswordSafetyCheckState::kSafe) {
      SafetyCheckItemView* passwordView = [[SafetyCheckItemView alloc]
          initWithItemType:SafetyCheckItemType::kPassword
                layoutType:SafetyCheckItemLayoutType::kCompact];

      passwordView.tapDelegate = self;

      [safetyCheckItems addObject:passwordView];
    }

    // Safe Browsing check
    //
    // NOTE: Don't add the Safe Browsing check if two items already exist in
    // `safetyCheckItems`. At most, the compact view displays two rows of items.
    if ([safetyCheckItems count] < 2 &&
        _state.safeBrowsingState != SafeBrowsingSafetyCheckState::kSafe) {
      SafetyCheckItemView* safeBrowsingView = [[SafetyCheckItemView alloc]
          initWithItemType:SafetyCheckItemType::kSafeBrowsing
                layoutType:SafetyCheckItemLayoutType::kCompact];

      safeBrowsingView.tapDelegate = self;

      [safetyCheckItems addObject:safeBrowsingView];
    }

    MultiRowContainerView* multiRowContainer =
        [[MultiRowContainerView alloc] initWithViews:safetyCheckItems];

    [self addSubview:multiRowContainer];

    AddSameConstraints(multiRowContainer, self);

    return;
  }

  // Show hero-cell view for single check issue.
  SafetyCheckItemView* view;

  if (_state.updateChromeState != UpdateChromeSafetyCheckState::kUpToDate) {
    view = [[SafetyCheckItemView alloc]
        initWithItemType:SafetyCheckItemType::kUpdateChrome
              layoutType:SafetyCheckItemLayoutType::kHero];
  }

  if (_state.passwordState != PasswordSafetyCheckState::kSafe) {
    view = [[SafetyCheckItemView alloc]
        initWithItemType:SafetyCheckItemType::kPassword
              layoutType:SafetyCheckItemLayoutType::kHero];
  }

  if (_state.safeBrowsingState != SafeBrowsingSafetyCheckState::kSafe) {
    view = [[SafetyCheckItemView alloc]
        initWithItemType:SafetyCheckItemType::kSafeBrowsing
              layoutType:SafetyCheckItemLayoutType::kHero];
  }

  view.tapDelegate = self;

  [self addSubview:view];

  AddSameConstraints(view, self);
}

@end
