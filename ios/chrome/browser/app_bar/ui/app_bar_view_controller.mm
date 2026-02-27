// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The font size for the tab count label.
const CGFloat kTabGridFontSize = 11;
// The size of the button images.
const CGFloat kButtonImageSize = 23;
// The padding between the image and the text in the buttons.
const CGFloat kButtonImagePadding = 3;
// The shadow radius for the buttons.
const CGFloat kButtonShadowRadius = 3;
// The shadow opacity for the buttons.
const CGFloat kButtonShadowOpacity = 0.2;
// The shadow offset for the buttons.
const CGFloat kButtonShadowOffset = 1;
// The duration of the animation to update the TabGrid button.
const CGFloat kTabGridAnimationDuration = 0.25;

// Returns the configuration for all the symbols.
UIImageSymbolConfiguration* AppBarSymbolConfiguration() {
  return [UIImageSymbolConfiguration
      configurationWithPointSize:kButtonImageSize
                          weight:UIImageSymbolWeightSemibold
                           scale:UIImageSymbolScaleMedium];
}

// Returns a default symbol with the common configuration.
UIImage* DefaultAppBarSymbol(NSString* symbol_name) {
  return DefaultSymbolWithConfiguration(symbol_name,
                                        AppBarSymbolConfiguration());
}

// Remove the "unused-function" check as this is only used when some buildflag
// is enabled.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
// Returns a custom symbol with the common configuration.
UIImage* CustomAppBarSymbol(NSString* symbol_name) {
  return CustomSymbolWithConfiguration(symbol_name,
                                       AppBarSymbolConfiguration());
}
#pragma clang diagnostic pop

}  // namespace

@implementation AppBarViewController {
  UIButton* _assistantButton;
  UIButton* _openNewTabButton;
  UIButton* _tabGridButton;
  UIImageView* _tabGridSymbolView;
  UILabel* _tabCountLabel;
  NSUInteger _tabCount;
  // Whether the tab grid is currently visible.
  BOOL _isTabGridVisible;
  // Whether the tab groups page in the tab grid is currently visible.
  BOOL _isTabGroupsPageVisible;
}

- (void)updateForAngle:(CGFloat)angle {
  [self loadViewIfNeeded];

  CGAffineTransform transform = CGAffineTransformMakeRotation(angle);
  _assistantButton.transform = transform;
  _openNewTabButton.transform = transform;
  _tabGridButton.transform = transform;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // TODO(crbug.com/483998773): Use a real design.
  self.view.backgroundColor = [UIColor.purpleColor colorWithAlphaComponent:0.5];

  _assistantButton = [self createAssistantButton];
  _openNewTabButton = [self createOpenNewTabButton];
  _tabGridButton = [self createTabGridButton];

  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    _assistantButton, _openNewTabButton, _tabGridButton
  ]];
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.distribution = UIStackViewDistributionFillEqually;

  [self.view addSubview:stackView];

  AddSameConstraints(stackView, self.view);

  [self.layoutGuideCenter referenceView:self.view underName:kAppBarGuide];
}

#pragma mark - AppBarConsumer

- (void)updateTabCount:(NSUInteger)count {
  _tabCount = count;
  _tabCountLabel.attributedText = TextForTabCount(count, kTabGridFontSize);
}

- (void)setTabGridVisible:(BOOL)tabGridVisible {
  _isTabGridVisible = tabGridVisible;
  [self updateTabGridButtonForTabGridShowing:tabGridVisible];
}

- (void)setTabGroupsPageVisible:(BOOL)tabGroupsPageVisible {
  if (tabGroupsPageVisible == _isTabGroupsPageVisible) {
    return;
  }
  _isTabGroupsPageVisible = tabGroupsPageVisible;
  [self updateNewTabButtonForTabGroupsPageVisibility];
}

#pragma mark - Private

// Returns a new "Assistant" button.
- (UIButton*)createAssistantButton {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_DIAMOND_PROTOTYPE_ASK_GEMINI);
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  UIImage* image = CustomAppBarSymbol(kGeminiBrandedLogoSymbol);
#else
  UIImage* image = DefaultAppBarSymbol(kGeminiNonBrandedLogoSymbol);
#endif
  UIButton* button = [self buttonWithTitle:title image:image];

  [button addTarget:self
                action:@selector(didTapAssistantButton)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Returns a new "New Tab" button.
- (UIButton*)createOpenNewTabButton {
  NSString* title = l10n_util::GetNSString(IDS_IOS_DIAMOND_PROTOTYPE_NEW_TAB);
  UIImage* image = DefaultAppBarSymbol(kPlusInCircleSymbol);
  UIButton* button = [self buttonWithTitle:title image:image];

  [button addTarget:self
                action:@selector(didTapOpenNewTabButton:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Returns a new "TabGrid" button.
- (UIButton*)createTabGridButton {
  NSString* title = l10n_util::GetNSString(IDS_IOS_DIAMOND_PROTOTYPE_ALL_TABS);
  UIImage* image = DefaultAppBarSymbol(kAppSymbol);
  UIButton* button = [self buttonWithTitle:title image:image];

  UIButtonConfiguration* configuration = button.configuration;
  // Make the base image clear so we can overlay our own with the label while
  // keeping the right size.
  configuration.imageColorTransformer = ^UIColor*(UIColor* color) {
    return UIColor.clearColor;
  };
  button.configuration = configuration;

  [button addTarget:self
                action:@selector(tabGridButtonTouchDown)
      forControlEvents:UIControlEventTouchDown];
  [button addTarget:self
                action:@selector(didTapTabGridButton)
      forControlEvents:UIControlEventTouchUpInside];

  // Use a custom Symbol and Label instead of the ones from the button to be
  // able to modify them as necessary.
  _tabGridSymbolView = [[UIImageView alloc] init];
  _tabGridSymbolView.translatesAutoresizingMaskIntoConstraints = NO;
  _tabGridSymbolView.tintColor = UIColor.whiteColor;
  _tabGridSymbolView.image = DefaultAppBarSymbol(kAppSymbol);
  [button addSubview:_tabGridSymbolView];
  AddSameCenterConstraints(_tabGridSymbolView, button.imageView);

  _tabCountLabel = [[UILabel alloc] init];
  _tabCountLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _tabCountLabel.textColor = UIColor.whiteColor;
  [self updateTabCount:_tabCount];
  [button addSubview:_tabCountLabel];
  AddSameCenterConstraints(_tabCountLabel, button.imageView);

  return button;
}

// Creates a new button with `title` and `image`.
- (UIButton*)buttonWithTitle:(NSString*)title image:(UIImage*)image {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.imagePlacement = NSDirectionalRectEdgeTop;
  configuration.imagePadding = kButtonImagePadding;
  configuration.baseForegroundColor = UIColor.whiteColor;

  configuration.image = image;
  configuration.title = title;

  UIButton* button = [UIButton buttonWithConfiguration:configuration
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset = CGSizeMake(0, kButtonShadowOffset);
  button.layer.shadowRadius = kButtonShadowRadius;
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.masksToBounds = NO;

  return button;
}

// Updates the new tab button for whether the tab groups page in the tab grid is
// showing.
- (void)updateNewTabButtonForTabGroupsPageVisibility {
  if (_isTabGroupsPageVisible) {
    __weak __typeof(self) weakSelf = self;
    UIButton* openNewTabButton = _openNewTabButton;

    UIAction* newTabGroupAction = [UIAction
        actionWithTitle:l10n_util::GetNSString(
                            IDS_IOS_APP_BAR_CONTEXT_MENU_NEW_TAB_GROUP)
                  image:DefaultSymbolWithConfiguration(kNewTabGroupActionSymbol,
                                                       nil)
             identifier:nil
                handler:^(UIAction*) {
                  [weakSelf
                      didTapContextMenuButtonNewTabGroup:openNewTabButton];
                }];

    UIAction* newTabAction = [UIAction
        actionWithTitle:l10n_util::GetNSString(
                            IDS_IOS_DIAMOND_PROTOTYPE_NEW_TAB)
                  image:DefaultSymbolWithConfiguration(kPlusSymbol, nil)
             identifier:nil
                handler:^(UIAction*) {
                  [weakSelf didTapContextMenuButtonNewTab:openNewTabButton];
                }];

    UIMenu* contextMenu =
        [UIMenu menuWithChildren:@[ newTabGroupAction, newTabAction ]];
    _openNewTabButton.menu = contextMenu;
    _openNewTabButton.showsMenuAsPrimaryAction = YES;
    return;
  }

  _openNewTabButton.menu = nil;
  _openNewTabButton.showsMenuAsPrimaryAction = NO;
}

// Updates the tab grid button for the given tab grid showing state.
- (void)updateTabGridButtonForTabGridShowing:(BOOL)showing {
  NSString* symbolName = showing ? kAppFillSymbol : kAppSymbol;
  [_tabGridSymbolView setSymbolImage:DefaultAppBarSymbol(symbolName)
               withContentTransition:[NSSymbolReplaceContentTransition
                                         replaceOffUpTransition]];
  UILabel* label = _tabCountLabel;
  UIColor* labelColor = showing ? UIColor.blackColor : UIColor.whiteColor;
  [UIView transitionWithView:label
                    duration:kTabGridAnimationDuration
                     options:UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
                    label.textColor = labelColor;
                  }
                  completion:nil];
}

// Called when the Assistant button is tapped.
- (void)didTapAssistantButton {
  base::RecordAction(base::UserMetricsAction("MobileToolbarAssistant"));
  [self.sceneHandler showAssistant];
}

// Called when the New Tab button is tapped.
- (void)didTapOpenNewTabButton:(UIView*)sender {
  base::RecordAction(base::UserMetricsAction("MobileToolbarNewTabShortcut"));
  [self.mutator createNewTabFromView:sender];
}

// Called when the New Tab button is selected from the context menu of the
// `_openNewTabButton` on the tab groups page of the tab grid.
- (void)didTapContextMenuButtonNewTab:(UIView*)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabGridCreateRegularTab"));
  [self.mutator createNewTabFromView:sender];
}

// Called when the New Tab Group button is selected from the context menu of the
// `_openNewTabButton` on the tab groups page of the tab grid.
- (void)didTapContextMenuButtonNewTabGroup:(UIView*)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabGridCreateTabGroup"));
  [self.mutator createNewTabGroupFromView:sender];
}

// Called when the Tab Grid button has a touch down.
- (void)tabGridButtonTouchDown {
  [IntentDonationHelper donateIntent:IntentType::kOpenTabGrid];
  [self.sceneHandler prepareTabSwitcher];
}

// Called when the Tab Grid button is tapped.
- (void)didTapTabGridButton {
  if (_isTabGridVisible) {
    base::RecordAction(base::UserMetricsAction("MobileTabGridDone"));
    [self.tabGridHandler exitTabGrid];
  } else {
    base::RecordAction(base::UserMetricsAction("MobileToolbarShowStackView"));
    [self.sceneHandler displayTabGridInMode:TabGridOpeningMode::kDefault];
  }
}

@end
