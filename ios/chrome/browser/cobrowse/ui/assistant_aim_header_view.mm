// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_header_view.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_mutator.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/image/g_with_point_size.h"
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
// The margin between the ends of the header actions pill and its buttons.
// There is no vertical margin: the buttons fill the pill's height, so that
// their touch targets are as tall as the pill.
const CGFloat kStackViewHorizontalMargin = 5.0;
// The size of the logo view.
const CGFloat kLogoSize = 32.0;

// The padding between the close button and the header actions.
const CGFloat kHeaderInnerPadding = 10;

// Shadow opacity for the glass effect container.
const CGFloat kGlassShadowOpacity = 0.07;

// Shadow radius for the glass effect container.
const CGFloat kGlassShadowRadius = 3;

// Vertical shadow offset for the glass effect container.
const CGFloat kGlassShadowOffsetY = 1;

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

// Applies the shadow for the header elements.
void ApplyHeaderElementShadow(UIView* targetView) {
  targetView.layer.shadowColor = [UIColor blackColor].CGColor;
  targetView.layer.shadowOpacity = kGlassShadowOpacity;
  targetView.layer.shadowOffset = CGSizeMake(0, kGlassShadowOffsetY);
  targetView.layer.shadowRadius = kGlassShadowRadius;
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

  // The stack view holding the trailing elements of the header, i.e. the header
  // actions and the close button.
  UIStackView* _trailingStackView;

  // The current mode of the header view.
  AssistantAIMState _mode;

  // The last adjusted percentage.
  CGFloat _percentage;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _mode = AssistantAIMState::kThread;
    _percentage = 1.0;
    [self addInteraction:[[UILargeContentViewerInteraction alloc] init]];
    [self setUpLogoView];
    [self setUpCloseButton];
    [self setUpHeaderActionsView];
    [self setUpTrailingStackView];
    [self setUpTitleLabel];
    [self setUpBackButton];
  }

  return self;
}

- (void)setTitle:(NSString*)title {
  _titleLabel.text = title;
}

- (void)adjustForPercentage:(CGFloat)percentage {
  _percentage = percentage;
  [self updateTitleAlpha];
  _headerActionsView.alpha = percentage;

  // Collapse the actions out of `_trailingStackView` once they are fully
  // transparent, so that the title can use the space they occupy.
  BOOL actionsCollapsed = percentage == 0;
  _headerActionsView.hidden = actionsCollapsed;
}

- (void)setMode:(AssistantAIMState)mode {
  _mode = mode;
  [self updateTitleAlpha];
  switch (mode) {
    case AssistantAIMState::kZeroState:
      _logoView.hidden = NO;
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

// Updates the alpha of the title label based on the current mode and
// percentage.
- (void)updateTitleAlpha {
  _titleLabel.alpha =
      (_mode == AssistantAIMState::kHistory) ? 1.0 : (1 - _percentage);
}

- (void)setUpTitleLabel {
  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _titleLabel.adjustsFontForContentSizeCategory = YES;
  _titleLabel.maximumContentSizeCategory =
      UIContentSizeCategoryExtraExtraExtraLarge;
  _titleLabel.showsLargeContentViewer = YES;
  _titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  _titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);
  _titleLabel.isAccessibilityElement = YES;
  _titleLabel.accessibilityIdentifier =
      kAssistantAIMTitleLabelAccessibilityIdentifier;
  [self updateTitleAlpha];
  [self addSubview:_titleLabel];

  [NSLayoutConstraint activateConstraints:@[
    [_titleLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [_titleLabel.leadingAnchor constraintEqualToAnchor:_logoView.trailingAnchor
                                              constant:kTitleLeadingPadding],
    [_titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:_trailingStackView.leadingAnchor
                                 constant:-kTitleLeadingTrailingPadding],
  ]];
}

// Sets up the stack view holding the trailing elements of the header. Grouping
// them allows the title to extend over the header actions when they are
// collapsed in the minimized state.
- (void)setUpTrailingStackView {
  _trailingStackView = [[UIStackView alloc] init];

  if (_headerActionsView) {
    [_trailingStackView addArrangedSubview:_headerActionsView];
  }

  [_trailingStackView addArrangedSubview:_closeButton];
  _trailingStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _trailingStackView.axis = UILayoutConstraintAxisHorizontal;
  _trailingStackView.alignment = UIStackViewAlignmentCenter;
  _trailingStackView.spacing = kHeaderInnerPadding;

  [self addSubview:_trailingStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_trailingStackView.centerYAnchor
        constraintEqualToAnchor:self.centerYAnchor],
    [_trailingStackView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kHorizontalPadding.right],
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
  _closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ICON_CLOSE);
  _closeButton.showsLargeContentViewer = YES;
  _closeButton.scalesLargeContentImage = YES;
  _closeButton.largeContentTitle = _closeButton.accessibilityLabel;

  // Shadow for button.
  ApplyHeaderElementShadow(_closeButton);

  AddSizeConstraints(_closeButton, CGSizeMake(kButtonSize, kButtonSize));
}

- (void)setUpBackButton {
  UIImage* image = SymbolTemplateWithPointSize(SymbolChevronBackward,
                                               kCloseButtonSymbolPointSize);
  UIButtonConfiguration* buttonConfiguration =
      CreateHeaderButtonConfiguration(image);

  _backButton =
      [ExtendedTouchTargetButton buttonWithConfiguration:buttonConfiguration
                                           primaryAction:nil];
  [_backButton addTarget:self
                  action:@selector(didTapBackButton)
        forControlEvents:UIControlEventTouchUpInside];
  _backButton.translatesAutoresizingMaskIntoConstraints = NO;
  _backButton.tintColor = [UIColor clearColor];
  _backButton.hidden = YES;
  _backButton.accessibilityIdentifier =
      kAssistantAIMBackButtonAccessibilityIdentifier;
  _backButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ICON_ARROW_BACK);
  _backButton.showsLargeContentViewer = YES;
  _backButton.scalesLargeContentImage = YES;
  _backButton.largeContentTitle = _backButton.accessibilityLabel;

  ApplyHeaderElementShadow(_backButton);
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
  button.showsLargeContentViewer = YES;
  button.scalesLargeContentImage = YES;
  AddSizeConstraints(button, CGSizeMake(kButtonSize, kButtonSize));
  return button;
}

// Creates the new thread button in header.
- (UIButton*)createStartThreadButton {
  CHECK(IsAssistantAimThreadsEnabled());
  UIButton* button = [self
      createHeaderActionButtonWithImage:SymbolTemplateWithPointSize(
                                            SymbolSquareAndPencil,
                                            kHeaderActionSymbolPointSize)];
  [button addTarget:self
                action:@selector(didTapStartNewThread)
      forControlEvents:UIControlEventTouchUpInside];
  button.hidden = NO;
  button.accessibilityIdentifier =
      kAssistantAIMNewThreadButtonAccessibilityIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(
      IDS_CONTEXTUAL_TASKS_SIDE_PANEL_NEW_THREAD_TOOL_TIP);
  button.largeContentTitle = button.accessibilityLabel;
  _startNewThreadButton = button;
  return button;
}

// Creates the history button in header.
- (UIButton*)createHistoryButton {
  CHECK(IsAssistantAimThreadsEnabled());
  UIButton* button = [self
      createHeaderActionButtonWithImage:SymbolTemplateWithPointSize(
                                            SymbolLineThreeSpark,
                                            kHeaderActionSymbolPointSize)];
  button.hidden = NO;
  button.accessibilityIdentifier =
      kAssistantAIMHistoryButtonAccessibilityIdentifier;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_CONTEXTUAL_TASKS_SIDE_PANEL_HISTORY_TOOL_TIP);
  button.largeContentTitle = button.accessibilityLabel;
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
  button.accessibilityIdentifier =
      kAssistantAIMContextMenuButtonAccessibilityIdentifier;
  button.accessibilityLabel = l10n_util::GetNSString(
      IDS_CONTEXTUAL_TASKS_SIDE_PANEL_MORE_OPTIONS_TOOL_TIP);
  button.largeContentTitle = button.accessibilityLabel;

  _contextMenuButton = button;

  NSMutableArray* actions = [[NSMutableArray alloc] init];
  __weak __typeof(self) weakSelf = self;

#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  UIImage* myActivityIcon = SymbolWithPointSize(SymbolGoogleIconMonochrome,
                                                kHeaderActionSymbolPointSize);
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
  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:
          IsAssistantAimThreadsEnabled()
              ? @[ [self createStartThreadButton], [self createHistoryButton] ]
              : @[]];

  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    [stackView addArrangedSubview:[self createContextMenuButton]];
  }

  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  stackView.axis = UILayoutConstraintAxisHorizontal;
  stackView.layoutMargins = UIEdgeInsetsMake(0, kStackViewHorizontalMargin, 0,
                                             kStackViewHorizontalMargin);
  stackView.layoutMarginsRelativeArrangement = YES;
  stackView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  return stackView;
}

// Sets up the view containing the header actions.
- (void)setUpHeaderActionsView {
  if (!IsAssistantAimThreadsEnabled() &&
      !experimental_flags::IsOmniboxDebuggingEnabled()) {
    return;
  }

  UIStackView* stackView = [self createHeaderActionsStackView];

  _headerActionsView = [[UIView alloc] init];
  if (@available(iOS 26, *)) {
    UIGlassEffect* glassEffect =
        [UIGlassEffect effectWithStyle:UIGlassEffectStyleRegular];
    glassEffect.interactive = YES;
    glassEffect.tintColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    UIVisualEffectView* glassContainer =
        [[UIVisualEffectView alloc] initWithEffect:glassEffect];

    [glassContainer.contentView addSubview:stackView];
    glassContainer.contentView.layer.cornerRadius = kButtonSize / 2;
    glassContainer.translatesAutoresizingMaskIntoConstraints = NO;
    glassContainer.layer.cornerRadius = kButtonSize / 2;
    glassContainer.clipsToBounds = YES;
    [_headerActionsView addSubview:glassContainer];
    AddSameConstraints(glassContainer, _headerActionsView);
  } else {
    // TODO(crbug.com/493128413): Implement iOS 18 specs once defined.
    [_headerActionsView addSubview:stackView];
  }

  _headerActionsView.layer.cornerRadius = kButtonSize / 2;
  _headerActionsView.translatesAutoresizingMaskIntoConstraints = NO;

  ApplyHeaderElementShadow(_headerActionsView);

  [NSLayoutConstraint activateConstraints:@[
    [_headerActionsView.heightAnchor constraintEqualToConstant:kButtonSize],
  ]];
  AddSameConstraints(_headerActionsView, stackView);
}

- (UIImage*)iconImage {
#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  return GWithPointSize(SuperGSize::k24pt);
#else
  return MakeSymbolMulticolor(SymbolWithPointSize(SymbolGearshape2, 24));
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
