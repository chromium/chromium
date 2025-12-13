// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/badge_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_button.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_button_factory.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_constants.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_item.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_view_visibility_delegate.h"
#import "ios/chrome/browser/infobars/model/badge_state.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// FullScreen progress threshold in which to toggle between full screen on and
// off mode for the badge view.
const CGFloat kFullScreenProgressThreshold = 0.85;

// Spacing between the top and trailing anchors of `unreadIndicatorView` and
// `displayedBadge`.
const CGFloat kUnreadIndicatorViewSpacing = 10.0;

// Height of `unreadIndicatorView`.
const CGFloat kUnreadIndicatorViewHeight = 6.0;

// Damping ratio of animating a change to the displayed badge.
const CGFloat kUpdateDisplayedBadgeAnimationDamping = 0.85;

// Width of the divider between badges.
CGFloat const kDividerWidthConstant = 1;

// Corner radius of the divider between badges.
CGFloat const kPSFSeparatorCornerRadius = 0.5;

// Vertical padding for separator.
CGFloat const kDividerVerticalPadding = 12.0;

}  // namespace

@interface BadgeViewController ()

// Button factory.
@property(nonatomic, strong) BadgeButtonFactory* buttonFactory;

// BadgeButton to show when in FullScreen (i.e. when the
// toolbars are expanded). Setting this property will add the button to the
// StackView.
@property(nonatomic, strong) BadgeButton* displayedBadge;

// StackView holding the displayedBadge.
@property(nonatomic, strong) UIStackView* stackView;

// View that displays a blue dot on the top-right corner of the displayed badge
// if there are unread badges to be shown in the overflow menu.
@property(nonatomic, strong) UIView* unreadIndicatorView;

@end

@implementation BadgeViewController {
  // Array of separator views displayed between badges (Proactive Suggestions
  // Framework enabled only).
  NSMutableArray<UIView*>* _separatorViews;
  // Array of currently displayed badges (Proactive Suggestions Framework
  // enabled only).
  NSArray<id<BadgeItem>>* _currentlyDisplayedBadges;
}

@synthesize forceDisabled = _forceDisabled;

- (instancetype)initWithButtonFactory:(BadgeButtonFactory*)buttonFactory {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    DCHECK(buttonFactory);
    _buttonFactory = buttonFactory;
    _stackView = [[UIStackView alloc] init];
    _separatorViews = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.stackView.axis = UILayoutConstraintAxisHorizontal;
  [self.view addSubview:self.stackView];
  AddSameConstraints(self.view, self.stackView);
}

#pragma mark BadgeConsumer

- (void)setupWithDisplayedBadge:(id<BadgeItem>)displayedBadgeItem {
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    [self clearAllBadges];
    if (displayedBadgeItem) {
      BadgeButton* newButton = [self.buttonFactory
          badgeButtonForBadgeType:displayedBadgeItem.badgeType
                     usingInfoBar:nil];
      [newButton setAccepted:displayedBadgeItem.badgeState & BadgeStateAccepted
                    animated:NO];
      [self.stackView addArrangedSubview:newButton];
      _currentlyDisplayedBadges = @[ displayedBadgeItem ];
    } else {
      _currentlyDisplayedBadges = @[];
    }
  } else {
    self.displayedBadge = nil;
    if (displayedBadgeItem) {
      BadgeButton* newButton = [self.buttonFactory
          badgeButtonForBadgeType:displayedBadgeItem.badgeType
                     usingInfoBar:nil];
      [newButton setAccepted:displayedBadgeItem.badgeState & BadgeStateAccepted
                    animated:NO];
      self.displayedBadge = newButton;
    }
  }

  [self.visibilityDelegate setBadgeViewHidden:!displayedBadgeItem];
}

- (void)updateDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
                     infoBar:(InfoBarIOS*)infoBar {
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    NSArray<id<BadgeItem>>* badges =
        displayedBadgeItem ? @[ displayedBadgeItem ] : @[];
    [self updateDisplayedBadges:badges];
  } else {
    if (displayedBadgeItem) {
      if (self.displayedBadge &&
          self.displayedBadge.badgeType == displayedBadgeItem.badgeType) {
        [self.displayedBadge
            setAccepted:displayedBadgeItem.badgeState & BadgeStateAccepted
               animated:YES];
      } else {
        BadgeButton* newButton = [self.buttonFactory
            badgeButtonForBadgeType:displayedBadgeItem.badgeType
                       usingInfoBar:infoBar];
        [newButton
            setAccepted:displayedBadgeItem.badgeState & BadgeStateAccepted
               animated:NO];
        self.displayedBadge = newButton;
      }
      [self.displayedBadge
          setEnabled:!(displayedBadgeItem.badgeState & BadgeStatePresented)];
    } else {
      self.displayedBadge = nil;
    }

    if (!self.forceDisabled) {
      [self.visibilityDelegate setBadgeViewHidden:!displayedBadgeItem];
    }
  }
}

- (void)markDisplayedBadgeAsRead:(BOOL)read {
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    return;
  }

  BadgeButton* firstBadge = [self badgeButtonAtIndex:0];
  if (!firstBadge) {
    return;
  }

  // Lazy init if the unread indicator needs to be shown.
  if (!self.unreadIndicatorView && !read) {
    // Add unread indicator to the displayed badge.
    self.unreadIndicatorView = [[UIView alloc] init];
    self.unreadIndicatorView.layer.cornerRadius =
        kUnreadIndicatorViewHeight / 2;
    self.unreadIndicatorView.backgroundColor = [UIColor colorNamed:kBlueColor];
    self.unreadIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
    self.unreadIndicatorView.accessibilityIdentifier =
        kBadgeUnreadIndicatorAccessibilityIdentifier;
    [firstBadge addSubview:self.unreadIndicatorView];
    [NSLayoutConstraint activateConstraints:@[
      [self.unreadIndicatorView.trailingAnchor
          constraintEqualToAnchor:firstBadge.trailingAnchor
                         constant:-kUnreadIndicatorViewSpacing],
      [self.unreadIndicatorView.topAnchor
          constraintEqualToAnchor:firstBadge.topAnchor
                         constant:kUnreadIndicatorViewSpacing],
      [self.unreadIndicatorView.heightAnchor
          constraintEqualToConstant:kUnreadIndicatorViewHeight],
      [self.unreadIndicatorView.heightAnchor
          constraintEqualToAnchor:self.unreadIndicatorView.widthAnchor]
    ]];
  }
  if (self.unreadIndicatorView) {
    self.unreadIndicatorView.hidden = read;
  }
}

- (void)setForceDisabled:(BOOL)forceDisabled {
  if (_forceDisabled == forceDisabled) {
    return;
  }
  _forceDisabled = forceDisabled;
  [self updateVisibility];
}

- (void)updateDisplayedBadges:(NSArray<id<BadgeItem>>*)badgesToDisplay {
  if (!IsProactiveSuggestionsFrameworkEnabled()) {
    [self updateDisplayedBadge:[badgesToDisplay firstObject] infoBar:nil];
    return;
  }

  _currentlyDisplayedBadges = [badgesToDisplay copy];
  [self clearAllBadges];

  // Iterate through badges and add them with separators.
  for (NSUInteger i = 0; i < badgesToDisplay.count; i++) {
    id<BadgeItem> badgeItem = badgesToDisplay[i];
    if (i > 0) {
      [self addSeparatorToStackView];
    }

    BadgeButton* badgeButton =
        [self.buttonFactory badgeButtonForBadgeType:badgeItem.badgeType
                                       usingInfoBar:nil];
    [badgeButton setAccepted:badgeItem.badgeState & BadgeStateAccepted
                    animated:NO];
    [badgeButton setEnabled:!(badgeItem.badgeState & BadgeStatePresented)];

    [self.stackView addArrangedSubview:badgeButton];
  }

  [self.visibilityDelegate setBadgeViewHidden:(badgesToDisplay.count == 0)];
}

#pragma mark FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  BOOL badgeViewShouldCollapse = progress <= kFullScreenProgressThreshold;
  if (badgeViewShouldCollapse) {
    [self.visibilityDelegate setBadgeViewHidden:YES];
  } else {
    CGFloat alphaValue = fmax((progress - kFullScreenProgressThreshold) /
                                  (1 - kFullScreenProgressThreshold),
                              0);
    [self updateVisibility];
    self.view.alpha = alphaValue;
  }
}

#pragma mark - Getter/Setter

- (void)setDisplayedBadge:(BadgeButton*)badgeButton {
  if (IsProactiveSuggestionsFrameworkEnabled()) {
    return;
  }

  if (badgeButton.badgeType == self.displayedBadge.badgeType) {
    return;
  }

  [self.stackView removeArrangedSubview:_displayedBadge];
  [_displayedBadge removeFromSuperview];
  if (!badgeButton) {
    _displayedBadge = nil;
    self.unreadIndicatorView = nil;
    return;
  }
  _displayedBadge = badgeButton;

  [self animateViewAppearance];
  [self.stackView addArrangedSubview:_displayedBadge];
}

#pragma mark - Private

// Returns the badge button at the specified index from stackView.
- (BadgeButton*)badgeButtonAtIndex:(NSInteger)index {
  NSInteger badgeIndex = 0;
  for (UIView* view in self.stackView.arrangedSubviews) {
    if ([view isKindOfClass:[BadgeButton class]]) {
      if (badgeIndex == index) {
        return (BadgeButton*)view;
      }
      badgeIndex++;
    }
  }
  return nil;
}

// Clears all badges and separators from the stackView.
- (void)clearAllBadges {
  for (UIView* view in self.stackView.arrangedSubviews) {
    [self.stackView removeArrangedSubview:view];
    [view removeFromSuperview];
  }

  for (UIView* separator in _separatorViews) {
    [separator removeFromSuperview];
  }
  [_separatorViews removeAllObjects];

  self.unreadIndicatorView = nil;
}

// Adds separator between badges.
- (void)addSeparatorToStackView {
  // TODO(crbug.com/462093487): Alternate badge/separator arranged subviews to
  // support 3+ badges.
  UIView* separator = [[UIView alloc] init];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  separator.isAccessibilityElement = NO;
  separator.backgroundColor = [UIColor colorNamed:kGrey300Color];
  separator.layer.cornerRadius = kPSFSeparatorCornerRadius;

  [self.view addSubview:separator];
  [_separatorViews addObject:separator];

  [NSLayoutConstraint activateConstraints:@[
    [separator.widthAnchor constraintEqualToConstant:kDividerWidthConstant],
    [separator.heightAnchor
        constraintEqualToAnchor:self.stackView.heightAnchor
                       constant:-(kDividerVerticalPadding * 2)],
    [separator.centerXAnchor
        constraintEqualToAnchor:self.stackView.centerXAnchor],
    [separator.centerYAnchor
        constraintEqualToAnchor:self.stackView.centerYAnchor]
  ]];
}

// Animates the badge view appearance.
- (void)animateViewAppearance {
  self.view.alpha = 0;
  self.view.transform = CGAffineTransformMakeScale(0.1, 0.1);
  [UIView animateWithDuration:kMaterialDuration2
                        delay:0
       usingSpringWithDamping:kUpdateDisplayedBadgeAnimationDamping
        initialSpringVelocity:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     self.view.alpha = 1;
                     self.view.transform = CGAffineTransformIdentity;
                   }
                   completion:nil];
}

// Update the visibility of `self.view` through `self.visibilityDelegate`.
- (void)updateVisibility {
  if (self.forceDisabled) {
    [self.visibilityDelegate setBadgeViewHidden:YES];
  } else {
    // Turning off force disable mode doesn't imply that the badge view will
    // not remain hidden. Check if there is a badge to be displayed to avoid
    // accidentally removing the placeholder as a side effect of unhiding.
    if (!IsProactiveSuggestionsFrameworkEnabled()) {
      [self.visibilityDelegate setBadgeViewHidden:!self.displayedBadge];
    } else {
      [self.visibilityDelegate
          setBadgeViewHidden:(_currentlyDisplayedBadges.count == 0)];
    }
  }
}

@end
