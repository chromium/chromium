// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"

#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
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
  UILabel* _tabCountLabel;
}

- (void)viewDidLoad {
  [super viewDidLoad];

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
}

#pragma mark - AppBarConsumer

- (void)updateTabCount:(NSUInteger)count {
  _tabCountLabel.attributedText = TextForTabCount(count, kTabGridFontSize);
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
                action:@selector(didTapOpenNewTabButton)
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
                action:@selector(didTapTabGridButton)
      forControlEvents:UIControlEventTouchUpInside];

  // Use a custom Symbol and Label instead of the ones from the button to be
  // able to modify them as necessary.
  UIImageView* symbolView = [[UIImageView alloc] init];
  symbolView.translatesAutoresizingMaskIntoConstraints = NO;
  symbolView.tintColor = UIColor.whiteColor;
  symbolView.image = DefaultAppBarSymbol(kAppSymbol);
  [button addSubview:symbolView];
  AddSameCenterConstraints(symbolView, button.imageView);

  _tabCountLabel = [[UILabel alloc] init];
  _tabCountLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _tabCountLabel.textColor = UIColor.whiteColor;
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

// Called when the Assistant button is tapped.
- (void)didTapAssistantButton {
  // TODO(crbug.com/472279443): Implement.
}

// Called when the New Tab button is tapped.
- (void)didTapOpenNewTabButton {
  [self.mutator createNewTab];
}

// Called when the Tab Grid button is tapped.
- (void)didTapTabGridButton {
  // TODO(crbug.com/472279443): Implement.
}

@end
