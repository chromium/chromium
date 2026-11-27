// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui/vertical_sidebar_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/vertical_sidebar_view.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_actions.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_button_visibility.h"
#import "ios/chrome/browser/toolbar/ui/buttons/toolbar_tab_grid_badge_button.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation VerticalSidebarViewController {
  ToolbarButtonFactory* _buttonFactory;
  ToolbarButtonActions* _actions;
  LayoutGuideCenter* _layoutGuideCenter;
  ToolbarButton* _backButton;
  ToolbarButton* _forwardButton;
  ToolbarButton* _newTabButton;
  // TODO(crbug.com/585252907): Highlight it during the Guided Tour's NTP step.
  // The toolbar coordinators' `-highlightViewInStep:` highlights their own tab
  // grid button, which is hidden while the sidebar is shown.
  ToolbarTabGridBadgeButton* _tabGridButton;
  ToolbarButton* _toolsMenuButton;
  // `PopupMenuUIUpdating` state received before the view is loaded.
  BOOL _toolsMenuIPHHighlighted;
  BOOL _overflowMenuHasBlueDot;
  // Whether the active tab is controlled by the actor.
  BOOL _actorControlled;
  // Recognizer of the taps on the disabled navigation buttons while the active
  // tab is actor-controlled.
  UIGestureRecognizer* _disabledNavigationTapRecognizer;
}

@dynamic view;

- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)buttonFactory
                              actions:(ToolbarButtonActions*)actions
                    layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _buttonFactory = buttonFactory;
    _actions = actions;
    _layoutGuideCenter = layoutGuideCenter;
  }
  return self;
}

#pragma mark - Properties

- (void)setActive:(BOOL)active {
  if (_active == active) {
    return;
  }
  _active = active;
  [self loadViewIfNeeded];
  [self updateLayoutGuides];
}

#pragma mark - UIViewController

- (void)loadView {
  VerticalSidebarView* view =
      [[VerticalSidebarView alloc] initWithFrame:CGRectZero];
  view.accessibilityIdentifier = kVerticalSidebarIdentifier;
  view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.view = view;

  _backButton = [_buttonFactory makeBackButton];
  [self setUpButton:_backButton ofType:ToolbarButtonTypeBack];
  _forwardButton = [_buttonFactory makeForwardButton];
  [self setUpButton:_forwardButton ofType:ToolbarButtonTypeForward];
  _newTabButton = [_buttonFactory makeNewTabButton];
  [self setUpButton:_newTabButton ofType:ToolbarButtonTypeNewTab];
  _tabGridButton = [_buttonFactory makeTabGridButton];
  [self setUpButton:_tabGridButton ofType:ToolbarButtonTypeTabGrid];
  _toolsMenuButton = [_buttonFactory makeToolsMenuButton];
  [self setUpButton:_toolsMenuButton ofType:ToolbarButtonTypeTools];
  _toolsMenuButton.iphHighlighted = _toolsMenuIPHHighlighted;
  _toolsMenuButton.hasBlueDot = _overflowMenuHasBlueDot;

  // Grouped like the system vertical bar: navigation on top, then tabs and the
  // tools menu at the bottom.
  [view setTopGroups:@[ @[ _backButton, _forwardButton ] ]
        bottomGroups:@[
          @[ _newTabButton, _tabGridButton ], @[ _toolsMenuButton ]
        ]];

  // As in the toolbar, taps on the navigation buttons disabled by the actor
  // tell why they're disabled.
  if (IsActorEnabled()) {
    _disabledNavigationTapRecognizer =
        [_actions disabledNavigationTapRecognizerForButtons:@[
          _backButton, _forwardButton
        ]];
    _disabledNavigationTapRecognizer.enabled = _actorControlled;
    [view addGestureRecognizer:_disabledNavigationTapRecognizer];
  }
}

#pragma mark - ToolbarButtonsConsumer

- (void)setCanGoBack:(BOOL)canGoBack {
  _backButton.enabled = canGoBack;
}

- (void)setCanGoForward:(BOOL)canGoForward animated:(BOOL)animated {
  // The sidebar always shows its forward button, enabled or not.
  _forwardButton.enabled = canGoForward;
}

- (void)setShareEnabled:(BOOL)enabled {
  // The omnibox has the share button while the sidebar is shown.
}

- (void)setNewTabEnabled:(BOOL)enabled {
  _newTabButton.enabled = enabled;
}

- (void)setNTPVisible:(BOOL)ntpVisible
            isLoading:(BOOL)isLoading
      loadingProgress:(double)progress {
  // The sidebar has no progress bar, and the mediator records the NTP
  // metrics.
}

- (void)setMenu:(UIMenu*)menu forButtonType:(ToolbarButtonType)buttonType {
  switch (buttonType) {
    case ToolbarButtonTypeBack:
      _backButton.menu = menu;
      return;
    case ToolbarButtonTypeForward:
      _forwardButton.menu = menu;
      return;
    case ToolbarButtonTypeTabGrid:
      NOTREACHED() << "The tab grid menu is given to ToolbarButtonActions";
    case ToolbarButtonTypeNewTab:
      _newTabButton.menu = menu;
      return;
    case ToolbarButtonTypeReload:
    case ToolbarButtonTypeStop:
    case ToolbarButtonTypeShare:
    case ToolbarButtonTypeAssistant:
    case ToolbarButtonTypeTools:
      // Not in the sidebar, or without a menu.
      return;
  }
}

- (void)updateTabCount:(NSUInteger)tabCount {
  _tabGridButton.tabCount = tabCount;
}

- (void)setInTabGroup:(BOOL)inTabGroup {
  _tabGridButton.inTabGroup = inTabGroup;
}

- (void)setAssistantButtonVisible:(BOOL)visible enabled:(BOOL)enabled {
  // The sidebar has no assistant button.
}

- (void)setActorControlled:(BOOL)actorControlled {
  _actorControlled = actorControlled;
  _disabledNavigationTapRecognizer.enabled = actorControlled;
}

#pragma mark - PopupMenuUIUpdating

- (void)updateUIForOverflowMenuIPHDisplayed {
  _toolsMenuIPHHighlighted = YES;
  _toolsMenuButton.iphHighlighted = YES;
}

- (void)updateUIForIPHDismissed {
  _toolsMenuIPHHighlighted = NO;
  _toolsMenuButton.iphHighlighted = NO;
}

- (void)setOverflowMenuBlueDot:(BOOL)hasBlueDot {
  _overflowMenuHasBlueDot = hasBlueDot;
  _toolsMenuButton.hasBlueDot = hasBlueDot;
}

#pragma mark - Private

// Wires `button`, the sidebar's button of `type`, to the actions, and makes it
// look and show as the sidebar needs.
- (void)setUpButton:(ToolbarButton*)button ofType:(ToolbarButtonType)type {
  [_actions configureButton:button ofType:type];
  // The sidebar shows every button, whatever the size class or state (e.g. a
  // disabled forward button). `kAlways` leaves `hidden` as the factory's mask
  // set it, so show the button first.
  button.hidden = NO;
  button.visibilityMask = ToolbarButtonVisibility::kAlways;
  // The pills are the buttons' background. The old design's buttons have no
  // background, but would add a shadow on a style change.
  button.shadowAndBackgroundRemoved = YES;
}

// Gives the named layout guides to the buttons while the sidebar is active, or
// releases the ones they have otherwise.
- (void)updateLayoutGuides {
  NSDictionary<GuideName*, UIView*>* buttonsByGuide = @{
    kBackButtonGuide : _backButton,
    kForwardButtonGuide : _forwardButton,
    kTabSwitcherGuide : _tabGridButton,
    kToolsMenuGuide : _toolsMenuButton,
  };
  for (GuideName* guide in buttonsByGuide) {
    UIView* button = buttonsByGuide[guide];
    if (self.active) {
      [_layoutGuideCenter referenceView:button underName:guide];
    } else if ([_layoutGuideCenter referencedViewUnderName:guide] == button) {
      [_layoutGuideCenter referenceView:nil underName:guide];
    }
  }
}

@end
