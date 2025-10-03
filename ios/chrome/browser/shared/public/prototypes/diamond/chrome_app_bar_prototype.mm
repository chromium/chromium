// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/prototypes/diamond/chrome_app_bar_prototype.h"

#import "components/translate/core/browser/translate_manager.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/prototypes/diamond/diamond_grid_button.h"
#import "ios/chrome/browser/shared/public/prototypes/diamond/utils.h"
#import "ios/chrome/browser/shared/ui/symbols/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Height of the "gap" in the mask of the browser background. This needs to be
// twice the corner radius because the
// bezierPathWithRoundedRect:byRoundingCorners:cornerRadii: method requires the
// height to be at least twice the corner radius.
CGFloat BrowserBackgroundGapHeight() {
  return 2 * kDiamondBrowserCornerRadius;
}

// Duration of the animation.
const CGFloat kBackgroundTransitionTime = 0.25;

// Shadow constants.
const CGFloat kButtonShadowOffsetX = 0;
const CGFloat kButtonShadowOffsetY = 1;
const CGFloat kButtonShadowRadius = 3;
const CGFloat kButtonShadowOpacity = 0.2;

UIButtonConfiguration* ButtonConfiguration() {
  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.imagePlacement = NSDirectionalRectEdgeTop;
  configuration.imagePadding = 3;
  configuration.baseForegroundColor = UIColor.whiteColor;
  configuration.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [incoming mutableCopy];
    outgoing[NSFontAttributeName] =
        [UIFont systemFontOfSize:11 weight:UIFontWeightMedium];
    return outgoing;
  };

  return configuration;
}

// Configures the shadow for the given `button`.
void ConfigureButtonShadow(UIButton* button) {
  button.layer.shadowColor = [UIColor blackColor].CGColor;
  button.layer.shadowOffset =
      CGSizeMake(kButtonShadowOffsetX, kButtonShadowOffsetY);
  button.layer.shadowRadius = kButtonShadowRadius;
  button.layer.shadowOpacity = kButtonShadowOpacity;
  button.layer.masksToBounds = NO;
}

}  // namespace

@implementation ChromeAppBarPrototype {
  CAShapeLayer* _maskLayer;
  // Background when the tab grid is visible.
  UIVisualEffectView* _tabGridBackground;
  // Background when the browser is visible.
  UIView* _browserBackground;
  // The view used for the shadow of the browser.
  UIView* _browserShadowOutline;
  // The blur effect for the backgrounds.
  UIBlurEffect* _blurEffect;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    CHECK(IsDiamondPrototypeEnabled());

    self.backgroundColor = UIColor.whiteColor;

    _blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterialDark];

    _tabGridBackground =
        [[UIVisualEffectView alloc] initWithEffect:_blurEffect];
    _tabGridBackground.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_tabGridBackground];
    AddSameConstraints(self, _tabGridBackground);

    _browserBackground = [[UIView alloc] init];
    _browserBackground.backgroundColor = [UIColor colorWithWhite:0.49 alpha:1];
    _browserBackground.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_browserBackground];
    [NSLayoutConstraint activateConstraints:@[
      [_browserBackground.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [_browserBackground.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_browserBackground.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [_browserBackground.topAnchor
          constraintEqualToAnchor:self.topAnchor
                         constant:-BrowserBackgroundGapHeight()],
    ]];

    _browserShadowOutline = [[UIView alloc] init];
    _browserShadowOutline.translatesAutoresizingMaskIntoConstraints = NO;
    _browserShadowOutline.layer.shadowColor = [UIColor blackColor].CGColor;
    _browserShadowOutline.layer.shadowOpacity = 0.9;
    _browserShadowOutline.layer.shadowRadius = 24.0;
    _browserShadowOutline.layer.shadowOffset = CGSizeMake(0, 5);
    _browserShadowOutline.layer.masksToBounds = NO;
    [self addSubview:_browserShadowOutline];
    [NSLayoutConstraint activateConstraints:@[
      [self.leadingAnchor
          constraintEqualToAnchor:_browserShadowOutline.leadingAnchor],
      [self.trailingAnchor
          constraintEqualToAnchor:_browserShadowOutline.trailingAnchor],
      [self.topAnchor constraintEqualToAnchor:_browserShadowOutline.topAnchor
                                     constant:kDiamondBrowserCornerRadius],
    ]];

    _maskLayer = [CAShapeLayer layer];
    _maskLayer.fillRule = kCAFillRuleEvenOdd;
    _maskLayer.fillColor = [UIColor blackColor].CGColor;
    _browserBackground.layer.mask = _maskLayer;
    [self updateMask];

    UIButtonConfiguration* askGeminiConfiguration = ButtonConfiguration();
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    askGeminiConfiguration.image =
        GetCustomAppBarSymbol(kGeminiBrandedLogoImage);
#else
    askGeminiConfiguration.image =
        GetCustomAppBarSymbol(kGeminiNonBrandedLogoImage);
#endif  // BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)

    askGeminiConfiguration.title =
        l10n_util::GetNSString(IDS_IOS_DIAMOND_PROTOTYPE_ASK_GEMINI);
    _askGeminiButton = [UIButton buttonWithConfiguration:askGeminiConfiguration
                                           primaryAction:nil];
    _askGeminiButton.translatesAutoresizingMaskIntoConstraints = NO;
    ConfigureButtonShadow(_askGeminiButton);

    UIButtonConfiguration* openNewTabConfiguration = ButtonConfiguration();
    openNewTabConfiguration.image = GetDefaultAppBarSymbol(kPlusInCircleSymbol);
    openNewTabConfiguration.title =
        l10n_util::GetNSString(IDS_IOS_DIAMOND_PROTOTYPE_NEW_TAB);
    _openNewTabButton =
        [UIButton buttonWithConfiguration:openNewTabConfiguration
                            primaryAction:nil];
    _openNewTabButton.translatesAutoresizingMaskIntoConstraints = NO;
    ConfigureButtonShadow(_openNewTabButton);

    UIButtonConfiguration* tabGridConfiguration = ButtonConfiguration();
    // TODO(crbug.com/429955447): replace the symbol with a tab grid icon,
    // including number of tabs.
    tabGridConfiguration.image = GetDefaultAppBarSymbol(kAppSymbol);
    tabGridConfiguration.imageColorTransformer = ^UIColor*(UIColor* color) {
      return UIColor.clearColor;
    };
    tabGridConfiguration.title =
        l10n_util::GetNSString(IDS_IOS_DIAMOND_PROTOTYPE_ALL_TABS);
    _tabGridButton =
        [DiamondGridButton buttonWithConfiguration:tabGridConfiguration
                                     primaryAction:nil];
    [_tabGridButton setup];
    _tabGridButton.translatesAutoresizingMaskIntoConstraints = NO;
    ConfigureButtonShadow(_tabGridButton);

    UILayoutGuide* heightGuide = [[UILayoutGuide alloc] init];
    [self addLayoutGuide:heightGuide];
    [heightGuide.heightAnchor
        constraintEqualToConstant:kChromeAppBarPrototypeHeight]
        .active = YES;
    AddSameConstraints(self.safeAreaLayoutGuide, heightGuide);

    UIStackView* stackView = [[UIStackView alloc] initWithArrangedSubviews:@[
      _askGeminiButton, _openNewTabButton, _tabGridButton
    ]];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:stackView];
    [NSLayoutConstraint activateConstraints:@[
      [self.leadingAnchor constraintEqualToAnchor:stackView.leadingAnchor],
      [self.trailingAnchor constraintEqualToAnchor:stackView.trailingAnchor],
      [self.centerYAnchor constraintEqualToAnchor:heightGuide.centerYAnchor
                                         constant:-4],
    ]];

    NSNotificationCenter* notificationCenter =
        [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(didEnterTabGrid)
                               name:kDiamondEnterTabGridNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(didLeaveTabGrid)
                               name:kDiamondLeaveTabGridNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(longPressButton:)
                               name:kDiamondLongPressButton
                             object:nil];
  }
  return self;
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  CHECK(IsDiamondPrototypeEnabled());
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateMask];
  _browserShadowOutline.layer.shadowPath = [self browserShadowPath].CGPath;
}

- (void)setCurrentPage:(TabGridPage)currentPage {
  _currentPage = currentPage;
  if (currentPage == TabGridPageRegularTabs) {
    [_tabGridButton
        configureWithWebStateList:self.regularBrowser->GetWebStateList()];
  } else {
    [_tabGridButton
        configureWithWebStateList:self.incognitoBrowser->GetWebStateList()];
  }
}

#pragma mark - Private

// Callback when exiting the grid.
- (void)didLeaveTabGrid {
  __weak UIView* browserBackground = _browserBackground;
  __weak UIVisualEffectView* tabGridBackground = _tabGridBackground;
  [UIView animateWithDuration:kBackgroundTransitionTime
                   animations:^{
                     tabGridBackground.effect = nil;
                     browserBackground.alpha = 1;
                   }];
}

// Callback when entering the grid.
- (void)didEnterTabGrid {
  __weak UIView* browserBackground = _browserBackground;
  __weak UIVisualEffectView* tabGridBackground = _tabGridBackground;
  UIBlurEffect* blur = _blurEffect;
  [UIView animateWithDuration:kBackgroundTransitionTime
                   animations:^{
                     tabGridBackground.effect = blur;
                     browserBackground.alpha = 0;
                   }];
}

// Callback when there is a long press on the diamond button in the toolbar.
- (void)longPressButton:(NSNotification*)notification {
  id<BrowserProviderInterface> browserProvider =
      self.regularBrowser->GetSceneState().browserProviderInterface;
  Browser* browser = browserProvider.currentBrowserProvider.browser;

  web::WebState* activeWebState =
      browser->GetWebStateList()->GetActiveWebState();

  BOOL canManuallyTranslate =
      [self canManuallyTranslateForWebState:activeWebState];
  BOOL lensOverlayVisible =
      [self isLensOverlayVisibleForWebState:activeWebState];
  BOOL readerModeAvailable =
      [self isReaderModeEnabledForWebState:activeWebState];
  BOOL readerModeVisible = [self isReaderModeActiveForWebState:activeWebState];

  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();

  id<ReaderModeCommands> readerModeHandler =
      HandlerForProtocol(dispatcher, ReaderModeCommands);
  id<BrowserCoordinatorCommands> browserCoordinatorHandler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  id<LensOverlayCommands> lensOverlayHandler =
      HandlerForProtocol(dispatcher, LensOverlayCommands);

  UIButton* button = notification.object;
  UIAction* translate =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_TRANSLATE_INFOBAR_MODAL_TITLE)
                          image:CustomSymbolWithPointSize(
                                    kTranslateSymbol, kSymbolActionPointSize)
                     identifier:nil
                        handler:^(UIAction* action) {
                          [browserCoordinatorHandler showTranslate];
                        }];
  if (readerModeVisible || !canManuallyTranslate || lensOverlayVisible) {
    translate.attributes = UIMenuElementAttributesDisabled;
  }

  UIAction* readerMode;
  if (readerModeVisible) {
    readerMode = [UIAction
        actionWithTitle:l10n_util::GetNSString(
                            IDS_IOS_AI_HUB_HIDE_READER_MODE_LABEL)
                  image:DefaultSymbolWithPointSize(kReaderModeSymbolPostIOS18,
                                                   kSymbolActionPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [readerModeHandler hideReaderMode];
                }];
  } else {
    readerMode = [UIAction
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_AI_HUB_READER_MODE_LABEL)
                  image:DefaultSymbolWithPointSize(kReaderModeSymbolPostIOS18,
                                                   kSymbolActionPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [readerModeHandler
                      showReaderModeFromAccessPoint:ReaderModeAccessPoint::
                                                        kContextualChip];
                }];
    if (!readerModeAvailable) {
      readerMode.attributes = UIMenuElementAttributesDisabled;
    }
  }

  UIAction* lens = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_TOOLTIP_TEXT)
                image:CustomSymbolWithPointSize(kCameraLensSymbol,
                                                kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action) {
                [lensOverlayHandler
                    createAndShowLensUI:YES
                             entrypoint:LensOverlayEntrypoint::kOverflowMenu
                             completion:nil];
              }];
  button.menu = [UIMenu menuWithChildren:@[ translate, readerMode, lens ]];
}

// Whether `webState` can be manually translated.
- (BOOL)canManuallyTranslateForWebState:(web::WebState*)webState {
  if (!webState) {
    return NO;
  }

  auto* translateClient = ChromeIOSTranslateClient::FromWebState(webState);
  if (!translateClient) {
    return NO;
  }

  translate::TranslateManager* translateManager =
      translateClient->GetTranslateManager();
  return translateManager->CanManuallyTranslate(NO);
}

// Returns whether Lens Overlay is currently being displayed.
- (BOOL)isLensOverlayVisibleForWebState:(web::WebState*)webState {
  if (!webState) {
    return NO;
  }
  LensOverlayTabHelper* lensOverlayTabHelper =
      LensOverlayTabHelper::FromWebState(webState);
  return lensOverlayTabHelper &&
         lensOverlayTabHelper->IsLensOverlayUIAttachedAndAlive();
}

- (BOOL)isReaderModeActiveForWebState:(web::WebState*)webState {
  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(webState);
  return readerModeTabHelper && readerModeTabHelper->IsActive();
}

// Whether Reader mode is enabled.
- (BOOL)isReaderModeEnabledForWebState:(web::WebState*)webState {
  if (!webState) {
    return NO;
  }

  ReaderModeTabHelper* helper = ReaderModeTabHelper::FromWebState(webState);
  if (!helper || helper->CurrentPageDistillationAlreadyFailed()) {
    return NO;
  }

  return helper->CurrentPageIsEligibleForReaderMode();
}

// Updates the mask of the background blur.
- (void)updateMask {
  CGRect maskFrame = CGRectMake(0, 0, _browserBackground.bounds.size.width,
                                BrowserBackgroundGapHeight());

  UIBezierPath* path =
      [UIBezierPath bezierPathWithRect:_browserBackground.bounds];
  UIBezierPath* cutoutPath = [UIBezierPath
      bezierPathWithRoundedRect:maskFrame
              byRoundingCorners:UIRectCornerBottomLeft | UIRectCornerBottomRight
                    cornerRadii:CGSizeMake(kDiamondBrowserCornerRadius,
                                           kDiamondBrowserCornerRadius)];
  [path appendPath:cutoutPath];

  _maskLayer.frame = maskFrame;
  _maskLayer.path = path.CGPath;
}

// Returns the path for the shadow of the browser.
- (UIBezierPath*)browserShadowPath {
  CGFloat cornerRadius = kDiamondBrowserCornerRadius;
  CGFloat width = _browserShadowOutline.bounds.size.width;

  UIBezierPath* path = [UIBezierPath bezierPath];

  // Start at the bottom-left corner (just before the curve)
  [path moveToPoint:CGPointMake(0, 0)];

  // Add the bottom-left rounded corner
  [path addArcWithCenter:CGPointMake(cornerRadius, 0)
                  radius:cornerRadius
              startAngle:M_PI
                endAngle:M_PI_2
               clockwise:NO];

  // Add a line to the bottom-right corner
  [path addLineToPoint:CGPointMake(width - cornerRadius, cornerRadius)];

  // Add the bottom-right rounded corner
  [path addArcWithCenter:CGPointMake(width - cornerRadius, 0)
                  radius:cornerRadius
              startAngle:M_PI_2
                endAngle:0
               clockwise:NO];

  return path;
}

@end
