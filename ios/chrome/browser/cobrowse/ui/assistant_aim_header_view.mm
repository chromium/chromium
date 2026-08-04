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
const CGFloat kTitleLeadingTrailingPadding = 10.0;
const CGFloat kButtonSize = 40.0;
const CGFloat kStackViewMargin = 5.0;
// The size of the logo view.
const CGFloat kLogoSize = 32.0;

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

  // The new thread button.
  UIButton* _startNewThreadButton;

  // The back button for history.
  UIButton* _backButton;

  // The context menu button.
  UIButton* _contextMenuButton;

  // The history button.
  UIButton* _historyButton;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    [self setUpLogoView];
    [self setUpCloseButton];
    [self setUpHeaderActionsView];
    [self setUpTitleLabel];
    [self setUpBackButton];
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

- (void)setMode:(AssistantAIMState)mode {
  switch (mode) {
    case AssistantAIMState::kZeroState:
      _logoView.hidden = NO;
      _headerActionsView.hidden = NO;
      _backButton.hidden = YES;
      _startNewThreadButton.hidden = YES;
      _historyButton.hidden = NO;
      _contextMenuButton.hidden =
          !experimental_flags::IsOmniboxDebuggingEnabled();
      _titleLabel.text = @"";
      self.backgroundColor = [UIColor clearColor];
      break;
    case AssistantAIMState::kThread:
      _logoView.hidden = NO;
      _headerActionsView.hidden = NO;
      _backButton.hidden = YES;
      _startNewThreadButton.hidden = NO;
      _historyButton.hidden = NO;
      _contextMenuButton.hidden =
          !experimental_flags::IsOmniboxDebuggingEnabled();
      _titleLabel.text = @"";
      self.backgroundColor = [UIColor clearColor];
      break;
    case AssistantAIMState::kHistory:
      _logoView.hidden = YES;
      _headerActionsView.hidden = NO;
      _startNewThreadButton.hidden = NO;
      _backButton.hidden = NO;
      _historyButton.hidden = YES;
      _contextMenuButton.hidden = NO;
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
  [self addSubview:_titleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_titleLabel.leadingAnchor constraintEqualToAnchor:_logoView.trailingAnchor
                                              constant:kTitleLeadingPadding],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_headerActionsView.leadingAnchor
                                 constant:-kTitleLeadingTrailingPadding],
  ]];
}

- (void)setUpCloseButton {
  UIImage* image =
      SymbolTemplateWithPointSize(SymbolXMark, kCloseButtonSymbolPointSize);
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
  UIImage* image =
      SymbolWithPointSize(SymbolChevronBackward, kCloseButtonSymbolPointSize);
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
  _logoView.contentMode = UIViewContentModeScaleAspectFit;
  [self addSubview:_logoView];
  [NSLayoutConstraint activateConstraints:@[
    [_logoView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_logoView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                            constant:kHorizontalPadding.left],
  ]];
  AddSizeConstraints(_logoView, CGSizeMake(kLogoSize, kLogoSize));
}

- (UIButton*)createHeaderActionButtonWithImage:(UIImage*)image {
  UIButtonConfiguration* config =
      [UIButtonConfiguration plainButtonConfiguration];
  config.image = image;
  config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];

  UIButton* button = [UIButton buttonWithConfiguration:config
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [button.heightAnchor constraintEqualToConstant:kButtonSize],
  ]];
  return button;
}

// Creates the new thread button in header.
- (UIButton*)createStartThreadButton {
  UIButton* button = [self
      createHeaderActionButtonWithImage:SymbolTemplateWithPointSize(
                                            SymbolSquareAndPencil,
                                            kHeaderActionSymbolPointSize)];
  [button addTarget:self
                action:@selector(didTapStartNewThread)
      forControlEvents:UIControlEventTouchUpInside];
  button.hidden = NO;
  _startNewThreadButton = button;
  return button;
}

// Creates the history button in header.
- (UIButton*)createHistoryButton {
  UIButton* button = [self
      createHeaderActionButtonWithImage:SymbolTemplateWithPointSize(
                                            SymbolLineThreeSpark,
                                            kHeaderActionSymbolPointSize)];
  // TODO(crbug.com/493128413): Add accessibility identifier for history button.
  button.hidden = NO;
  [button addTarget:self
                action:@selector(didTapHistoryButton)
      forControlEvents:UIControlEventTouchUpInside];
  _historyButton = button;
  return button;
}

// Creates the context menu button in header.
- (UIButton*)createContextMenuButton {
  UIButton* button = [self
      createHeaderActionButtonWithImage:SymbolTemplateWithPointSize(
                                            SymbolMenu,
                                            kHeaderActionSymbolPointSize)];
  button.hidden = !experimental_flags::IsOmniboxDebuggingEnabled();
  button.accessibilityIdentifier =
      kAssistantAIMContextMenuButtonAccessibilityIdentifier;

  _contextMenuButton = button;

  NSMutableArray* actions = [[NSMutableArray alloc] init];
  __weak __typeof(self) weakSelf = self;

#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  UIImage* myActivityIcon = MakeSymbolMonochrome(
      SymbolWithPointSize(SymbolGoogleIcon, kHeaderActionSymbolPointSize));
#else
  UIImage* myActivityIcon =
      SymbolWithPointSize(SymbolInfoCircle, kHeaderActionSymbolPointSize);
#endif

  UIAction* myActivityAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_MY_ACTIVITY_TITLE)
                image:myActivityIcon
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf didTapMyActivityButton];
              }];
  [actions addObject:myActivityAction];

  UIAction* helpAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_HELP_MOBILE)
                image:SymbolWithPointSize(SymbolHelp,
                                          kHeaderActionSymbolPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [weakSelf didTapHelpButton];
              }];
  [actions addObject:helpAction];

  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    UIAction* showLogsAction = [UIAction
        actionWithTitle:@"AIM SRP Logs"
                  image:SymbolWithPointSize(SymbolBinocularsCircle, 16)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf didTapShowLogsButton];
                }];
    [actions addObject:showLogsAction];

    UIAction* showURLAction =
        [UIAction actionWithTitle:@"AIM Loaded URL"
                            image:SymbolWithPointSize(SymbolLinkAction, 16)
                       identifier:nil
                          handler:^(UIAction* action) {
                            [weakSelf didTapShowURLButton];
                          }];
    [actions addObject:showURLAction];
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
    [self createStartThreadButton], [self createHistoryButton],
    [self createContextMenuButton]
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
      SymbolWithPointSize(SymbolGoogleIcon, kSymbolsPointSize));
#else
  return MakeSymbolMulticolor(
      SymbolWithPointSize(SymbolGearshape2, kSymbolsPointSize));
#endif
}

#pragma mark - Actions

- (void)didTapCloseButton {
  [self.delegate assistantAIMHeaderViewDidPressClose:self];
}

- (void)didTapHistoryButton {
  [self.delegate assistantAIMHeaderViewDidTapHistory:self];
}

- (void)didTapMyActivityButton {
  [self.delegate assistantAIMHeaderViewDidTapMyActivity:self];
}

- (void)didTapHelpButton {
  [self.delegate assistantAIMHeaderViewDidTapHelp:self];
}

- (void)didTapShowLogsButton {
  [self.delegate assistantAIMHeaderViewDidRequestSRPLogs:self];
}

- (void)didTapShowURLButton {
  [self.delegate assistantAIMHeaderViewDidRequestLoadedURL:self];
}

- (void)didTapBackButton {
  [self.delegate assistantAIMHeaderViewDidTapBack:self];
}

- (void)didTapStartNewThread {
  [self.delegate assistantAIMHeaderViewDidTapStartNewThread:self];
}

@end
