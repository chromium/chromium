// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_header_view.h"

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_mutator.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The point size of the close button.
const CGFloat kCloseButtonSymbolPointSize = 15.0;
const CGFloat kHeaderActionSymbolPointSize = 17.0;

// The leading and trailing padding of the header view.
const UIEdgeInsets kHorizontalPadding = {.left = 22.0, .right = 16.0};
const CGFloat kTitleLeadingPadding = 18.0;
const CGFloat kButtonSize = 40.0;
const CGFloat kStackViewMargin = 5.0;

// The padding between the close button and the header actions.
const CGFloat kHeaderInnerPadding = 10;

// The logo point size.
const CGFloat kSymbolsPointSize = 24.0;

// Creates a button configuration for a header button.
UIButtonConfiguration* CreateHeaderButtonConfiguration(UIImage* image) {
  UIButtonConfiguration* config;
  if (@available(iOS 26, *)) {
    if ([UIButtonConfiguration
            respondsToSelector:@selector(prominentGlassButtonConfiguration)]) {
      config = [UIButtonConfiguration prominentGlassButtonConfiguration];
    } else {
      config = [UIButtonConfiguration glassButtonConfiguration];
    }
  } else {
    config = [UIButtonConfiguration plainButtonConfiguration];
  }

  config.image = image;
  config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];
  config.background.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;

  return config;
}

}  // namespace

@implementation AssistantAIMHeaderView {
  // The label representing the title of the header.
  UILabel* _titleLabel;

  // The close button.
  UIButton* _closeButton;

  // The logo.
  UIImageView* _logoView;

  // The view holding the actions.
  UIView* _headerActionsView;

  // The back button for history.
  UIButton* _backButton;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    [self setUpLogoView];
    [self setUpTitleLabel];
    [self setUpCloseButton];
    [self setUpBackButton];
    [self setUpHeaderActionsView];
  }

  return self;
}

- (void)setTitle:(NSString*)title {
  _titleLabel.text = title;
}

- (void)adjustForPercentage:(CGFloat)percentage {
  _titleLabel.alpha = 1 - percentage;
  _headerActionsView.alpha = percentage;
}

- (void)setMode:(AssistantAIMHeaderViewMode)mode {
  switch (mode) {
    case AssistantAIMHeaderViewMode::kChat:
      _logoView.hidden = NO;
      _headerActionsView.hidden = NO;
      _backButton.hidden = YES;
      _titleLabel.text = @"";

      self.backgroundColor = [UIColor clearColor];
      break;
    case AssistantAIMHeaderViewMode::kHistory:
      _logoView.hidden = YES;
      _headerActionsView.hidden = YES;
      _backButton.hidden = NO;
      _titleLabel.text = l10n_util::GetNSString(IDS_IOS_AIM_HISTORY);

      self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
      break;
  }
}

#pragma mark - Private

- (void)setUpTitleLabel {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);
  _titleLabel.isAccessibilityElement = YES;
  _titleLabel.adjustsFontSizeToFitWidth = YES;
  [self addSubview:_titleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_titleLabel.leadingAnchor constraintEqualToAnchor:_logoView.trailingAnchor
                                              constant:kTitleLeadingPadding],
  ]];
}

- (void)setUpCloseButton {
  UIImage* image = DefaultSymbolTemplateWithPointSize(
      kXMarkSymbol, kCloseButtonSymbolPointSize);
  UIButtonConfiguration* buttonConfiguration =
      CreateHeaderButtonConfiguration(image);

  _closeButton =
      [ExtendedTouchTargetButton buttonWithConfiguration:buttonConfiguration
                                           primaryAction:nil];
  [_closeButton addTarget:self
                   action:@selector(didTapCloseButton)
         forControlEvents:UIControlEventTouchUpInside];
  _closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  _closeButton.tintColor = [UIColor clearColor];
  _closeButton.accessibilityIdentifier =
      kAssistantAIMCloseButtonAccessibilityIdentifier;

  [self addSubview:_closeButton];

  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_closeButton.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kHorizontalPadding.right],
  ]];

  AddSizeConstraints(_closeButton, CGSizeMake(kButtonSize, kButtonSize));
}

- (void)setUpBackButton {
  UIImage* image = DefaultSymbolWithPointSize(kChevronBackwardSymbol,
                                              kCloseButtonSymbolPointSize);
  UIButtonConfiguration* buttonConfiguration =
      CreateHeaderButtonConfiguration(image);

  _backButton = [UIButton buttonWithConfiguration:buttonConfiguration
                                    primaryAction:nil];
  [_backButton addTarget:self
                  action:@selector(didTapBackButton)
        forControlEvents:UIControlEventTouchUpInside];
  _backButton.translatesAutoresizingMaskIntoConstraints = NO;
  _backButton.hidden = YES;

  [self addSubview:_backButton];

  [NSLayoutConstraint activateConstraints:@[
    [_backButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_backButton.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                              constant:kHorizontalPadding.left],
  ]];

  AddSizeConstraints(_backButton, CGSizeMake(kButtonSize, kButtonSize));
}

- (void)setUpLogoView {
  _logoView = [[UIImageView alloc] initWithImage:[self iconImage]];
  _logoView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_logoView];
  [NSLayoutConstraint activateConstraints:@[
    [_logoView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_logoView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                            constant:kHorizontalPadding.left],
  ]];
}

// Creates the new thread button in header.
- (UIButton*)createStartThreadButton {
  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];
  config.image = DefaultSymbolTemplateWithPointSize(
      kSquareAndPencilSymbol, kHeaderActionSymbolPointSize);
  config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];

  // TODO(crbug.com/493128413): Implement action.
  UIButton* button = [UIButton buttonWithConfiguration:config
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [button.heightAnchor constraintEqualToConstant:kButtonSize],
  ]];

  return button;
}

// Creates the context menu button in header.
- (UIButton*)createContextMenuButton {
  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];
  config.image = DefaultSymbolTemplateWithPointSize(
      kMenuSymbol, kHeaderActionSymbolPointSize);
  config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];

  // TODO(crbug.com/493128413): Implement missing actions.
  UIButton* button = [UIButton buttonWithConfiguration:config
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  NSMutableArray* actions = [[NSMutableArray alloc] init];

  if (IsCobrowseAimHistoryEnabled()) {
    __weak __typeof(self) weakSelf = self;
    UIAction* historyAction = [UIAction
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_AIM_HISTORY)
                  image:CustomSymbolWithPointSize(kLineThreeSparkSymbol,
                                                  kHeaderActionSymbolPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf didTapHistoryButton];
                }];
    [actions addObject:historyAction];
  }

  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    __weak __typeof(self) weakSelf = self;
    UIAction* showLogsAction = [UIAction
        actionWithTitle:@"AIM SRP Logs"
                  image:DefaultSymbolWithPointSize(@"binoculars.circle", 16)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf didTapShowLogsButton];
                }];
    [actions addObject:showLogsAction];
  }

  button.menu = [UIMenu menuWithTitle:@"" children:actions];

  button.showsMenuAsPrimaryAction = YES;

  [NSLayoutConstraint activateConstraints:@[
    [button.heightAnchor constraintEqualToConstant:kButtonSize],
  ]];

  return button;
}

// Builds the stack view of the header actions.
- (UIStackView*)createHeaderActionsStackView {
  UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    [self createStartThreadButton], [self createContextMenuButton]
  ]];

  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.layoutMargins = UIEdgeInsetsMake(
      kStackViewMargin, kStackViewMargin, kStackViewMargin, kStackViewMargin);
  stackView.layoutMarginsRelativeArrangement = YES;
  stackView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  return stackView;
}

// Sets up the view containing the header actions.
- (void)setUpHeaderActionsView {
  UIStackView* stackView = [self createHeaderActionsStackView];

  if (@available(iOS 26, *)) {
    UIGlassEffect* glassEffect =
        [UIGlassEffect effectWithStyle:UIGlassEffectStyleRegular];
    glassEffect.interactive = YES;
    glassEffect.tintColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    UIVisualEffectView* glassContainer =
        [[UIVisualEffectView alloc] initWithEffect:glassEffect];

    [glassContainer.contentView addSubview:stackView];
    _headerActionsView = glassContainer;
  } else {
    // TODO(crbug.com/493128413): Implement iOS 18 specs once defined.
    _headerActionsView = [[UIView alloc] init];
    [_headerActionsView addSubview:stackView];
  }

  _headerActionsView.translatesAutoresizingMaskIntoConstraints = NO;
  _headerActionsView.layer.cornerRadius = kButtonSize / 2;
  _headerActionsView.clipsToBounds = YES;

  [self addSubview:_headerActionsView];

  [NSLayoutConstraint activateConstraints:@[
    [_headerActionsView.trailingAnchor
        constraintEqualToAnchor:_closeButton.leadingAnchor
                       constant:-kHeaderInnerPadding],
    [_headerActionsView.centerYAnchor
        constraintEqualToAnchor:_closeButton.centerYAnchor],
    [_headerActionsView.heightAnchor constraintEqualToConstant:kButtonSize],
  ]];
  AddSameConstraints(_headerActionsView, stackView);
}

- (UIImage*)iconImage {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kGoogleIconSymbol, kSymbolsPointSize));
#else
  return MakeSymbolMulticolor(
      DefaultSymbolWithPointSize(kGearshape2Symbol, kSymbolsPointSize));
#endif
}

#pragma mark - Actions

- (void)didTapCloseButton {
  [self.delegate assistantAIMHeaderViewDidPressClose:self];
}

- (void)didTapHistoryButton {
  [self.actionHandler didTapHistory];
}

- (void)didTapShowLogsButton {
  [self.delegate assistantAIMHeaderViewDidRequestSRPLogs:self];
}

- (void)didTapBackButton {
  [self.delegate assistantAIMHeaderViewDidTapBack:self];
}

@end
