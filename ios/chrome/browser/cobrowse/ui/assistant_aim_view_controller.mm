// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"

#import <WebKit/WebKit.h>

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_header_view.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_history_item.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_history_view_controller.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_mutator.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Pattern tile size.
const CGSize kBarricadeTapeTileSize = {10.0, 10.0};
// Height of the barricade tape.
const CGFloat kBarricadeTapeHeight = 6.0;

constexpr CGFloat kInputPlateMargin = 10.0f;
constexpr CGFloat kTitleVerticalMargin = 12.0;
constexpr CGFloat kHeaderCenteringVerticalMargin = 16.0;
constexpr CGFloat kThresholdForClosedState = 0.12;
constexpr CGFloat kThresholdForCompleteVisibility = 0.3;

}  // namespace

@interface AssistantAIMViewController () <
    AssistantAIMHeaderViewDelegate,
    AssistantAIMHistoryViewControllerDelegate>
@end

@implementation AssistantAIMViewController {
  UIView* _webStateView;
  NSArray<NSLayoutConstraint*>* _webStateViewConstraints;
  ComposeboxInputPlateViewController* _inputViewController;
  // Fade added behind the input plate.
  UIView* _inputViewFade;
  CALayer* _fadeGradient;
  AssistantAIMHeaderView* _headerView;
  NSLayoutConstraint* _headerTopMargin;
  NSLayoutConstraint* _inputPlateBottomMargin;
  CGRect _keyboardFrameInWindow;
  AssistantAIMHistoryViewController* _historyViewController;
}

@synthesize delegate = _delegate;

- (void)setMutator:(id<AssistantAIMMutator>)mutator {
  _mutator = mutator;
  _headerView.actionHandler = mutator;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  if (experimental_flags::GetCobrowseGwsURL()) {
    [self setUpBarricadeTape];
  }
  [self setUpHeader];
  [self setUpWebStateView];

  [self
      registerForTraitChanges:
          @[ UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class ]
                   withAction:@selector(traitsDidChange)];
  [self traitsDidChange];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [_inputViewController.view layoutIfNeeded];
  _fadeGradient.frame = _inputViewFade.bounds;
  [self updateInputPlateOverlap];
}

- (void)traitsDidChange {
  [self.delegate assistantAIMViewControllerDidChangeTraits:self];
}

- (void)addInputViewController:
    (ComposeboxInputPlateViewController*)inputViewController {
  [self loadViewIfNeeded];
  [self addChildViewController:inputViewController];
  [self.view addSubview:inputViewController.view];
  inputViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [inputViewController didMoveToParentViewController:self];
  _inputViewController = inputViewController;

  [_inputViewController.view
      setContentHuggingPriority:UILayoutPriorityRequired
                        forAxis:UILayoutConstraintAxisVertical];
  // Allow compression on the input view to limit it's height in the available
  // space (between the keyboard and the top of the view).
  [_inputViewController.view
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];

  if (!UIAccessibilityIsReduceTransparencyEnabled()) {
    [self createInputViewFade];
    [self.view insertSubview:_inputViewFade
                belowSubview:_inputViewController.view];
  }

  [self setupInputPlateConstraints];
}

- (void)adjustForContainerOpenPercentage:(CGFloat)percentage {
  // The percentage to use for animations, that is proportional to the container
  // open percentage with more sudden thresholds.
  CGFloat effectPercentage;
  if (percentage < kThresholdForClosedState) {
    effectPercentage = 0;
  } else if (percentage > kThresholdForCompleteVisibility) {
    effectPercentage = 1;
  } else {
    effectPercentage =
        (percentage - kThresholdForClosedState) /
        (kThresholdForCompleteVisibility - kThresholdForClosedState);
  }

  // This ensures the header end up centered in the collapsed state.
  _headerTopMargin.constant =
      kHeaderCenteringVerticalMargin +
      effectPercentage *
          (kTitleVerticalMargin - kHeaderCenteringVerticalMargin);

  _inputViewController.view.alpha = effectPercentage;
  _webStateView.alpha = effectPercentage;
  _inputViewFade.alpha = effectPercentage;
  _inputViewController.view.hidden = (effectPercentage == 0);

  [_headerView adjustForPercentage:effectPercentage];
}

- (BOOL)shouldPauseScrollView:(UIScrollView*)scrollView
                   forGesture:(UIGestureRecognizer*)gesture
            isInLargestDetent:(BOOL)isInLargestDetent {
  // Only handle gestures in the assistant content.
  BOOL inAssistantContent = [scrollView isDescendantOfView:self.view];
  if (!inAssistantContent) {
    return NO;
  }

  // Only pause if the gesture controls scrolling.
  if (![gesture isKindOfClass:[UIPanGestureRecognizer class]]) {
    return NO;
  }

  // Safe cast because the check above ensures it's a pan gesture.
  UIPanGestureRecognizer* panRecognizer =
      static_cast<UIPanGestureRecognizer*>(gesture);

  // Horizontal scroll should not drag the assistant container.
  if ([self gestureDidScrollHorizontally:panRecognizer]) {
    return NO;
  }

  WKWebView* wkWebView = [self findWKWebViewInView:_webStateView];

  // Allow overscroll for the main scroll view and sub-scroll views (e.g.
  // iframes) if they are descendants of the web view.
  BOOL isDescendant = [scrollView isDescendantOfView:wkWebView];
  if (!isDescendant) {
    return NO;
  }

  // Check boundaries.
  CGFloat verticalVelocity = [panRecognizer velocityInView:scrollView].y;
  BOOL draggingDown = verticalVelocity > 0;

  // Check if the current scroll view is at the top.
  BOOL isAtTop = scrollView.contentOffset.y <= 0;

  BOOL sheetMovementForLargestDetent =
      isInLargestDetent && isAtTop && draggingDown;
  BOOL sheetMovementForSmallerDetents = !isInLargestDetent && isAtTop;

  if (sheetMovementForLargestDetent || sheetMovementForSmallerDetents) {
    return YES;
  }
  return NO;
}

- (void)setupInputPlateConstraints {
  _inputPlateBottomMargin = [_inputViewController.view.bottomAnchor
      constraintEqualToAnchor:self.view.bottomAnchor
                     constant:-kInputPlateMargin];

  [NSLayoutConstraint activateConstraints:@[
    _inputPlateBottomMargin,
    [_inputViewController.view.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kInputPlateMargin],
    [_inputViewController.view.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kInputPlateMargin],
  ]];

  if (_inputViewFade) {
    [NSLayoutConstraint activateConstraints:@[
      [_inputViewFade.topAnchor
          constraintEqualToAnchor:_inputViewController.view.topAnchor],
      [_inputViewFade.leadingAnchor
          constraintEqualToAnchor:self.view.leadingAnchor],
      [_inputViewFade.trailingAnchor
          constraintEqualToAnchor:self.view.trailingAnchor],
      [_inputViewFade.bottomAnchor
          constraintEqualToAnchor:self.view.bottomAnchor],
    ]];
  }

  // TODO(crbug.com/493187015): Investigate why `keyboardLayoutGuide` cannot be
  // used here. When the keyboard is hidden, `keyboardLayoutGuide.topAnchor`
  // currently pushes the container downwards below its intended position. We
  // manually observe keyboard frames and update constraints as a workaround.
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];

  [defaultCenter addObserver:self
                    selector:@selector(keyboardWillChangeFrame:)
                        name:UIKeyboardWillChangeFrameNotification
                      object:nil];

  [defaultCenter addObserver:self
                    selector:@selector(keyboardWillShow:)
                        name:UIKeyboardWillShowNotification
                      object:nil];

  [defaultCenter addObserver:self
                    selector:@selector(keyboardDidHide:)
                        name:UIKeyboardDidHideNotification
                      object:nil];
}

#pragma mark - AssistantAIMConsumer

- (void)setWebStateView:(UIView*)webStateView {
  if (_webStateView == webStateView) {
    return;
  }
  [_webStateView removeFromSuperview];
  _webStateView = webStateView;

  [self setUpWebStateView];
}

- (void)displayHistoryWithItems:
    (const std::vector<AssistantAIMHistoryItem>&)items {
  if (!_historyViewController) {
    _historyViewController = [[AssistantAIMHistoryViewController alloc] init];
    _historyViewController.delegate = self;

    [self addChildViewController:_historyViewController];

    _webStateView.hidden = YES;
    _inputViewController.view.hidden = YES;
    [_headerView setMode:AssistantAIMHeaderViewMode::kHistory];
    self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

    [self.view addSubview:_historyViewController.view];
    _historyViewController.view.translatesAutoresizingMaskIntoConstraints = NO;

    [NSLayoutConstraint activateConstraints:@[
      [_historyViewController.view.topAnchor
          constraintEqualToAnchor:_headerView.bottomAnchor],
      [_historyViewController.view.leadingAnchor
          constraintEqualToAnchor:self.view.leadingAnchor],
      [_historyViewController.view.trailingAnchor
          constraintEqualToAnchor:self.view.trailingAnchor],
      [_historyViewController.view.bottomAnchor
          constraintEqualToAnchor:self.view.bottomAnchor],
    ]];

    [_historyViewController didMoveToParentViewController:self];
  }

  [_historyViewController updateHistoryItems:items];
}

#pragma mark - AssistantAIMHistoryViewControllerDelegate

- (void)assistantAIMHistoryViewControllerDidTapDismiss:
    (AssistantAIMHistoryViewController*)viewController {
  [self hideHistory];
}

- (void)assistantAIMHistoryViewController:
            (AssistantAIMHistoryViewController*)viewController
                      didSelectTaskWithId:(NSString*)taskId {
  [self.mutator didSelectHistoryTaskWithId:taskId];
  [self hideHistory];
}

#pragma mark - Private

// Recursively searches for a WKWebView in the given view's hierarchy.
- (WKWebView*)findWKWebViewInView:(UIView*)view {
  if ([view isKindOfClass:[WKWebView class]]) {
    return static_cast<WKWebView*>(view);
  }
  for (UIView* subview in view.subviews) {
    WKWebView* webView = [self findWKWebViewInView:subview];
    if (webView) {
      return webView;
    }
  }
  return nil;
}

// Returns YES if the gesture has a mostly horizontal translation.
- (BOOL)gestureDidScrollHorizontally:(UIPanGestureRecognizer*)panRecognizer {
  CGPoint translation = [panRecognizer translationInView:panRecognizer.view];
  return fabs(translation.y) <= 3 * fabs(translation.x);
}

- (void)hideHistory {
  if (!_historyViewController) {
    return;
  }
  [_historyViewController willMoveToParentViewController:nil];
  [_historyViewController.view removeFromSuperview];
  [_historyViewController removeFromParentViewController];
  _historyViewController = nil;

  _webStateView.hidden = NO;
  _inputViewController.view.hidden = NO;
  [_headerView setMode:AssistantAIMHeaderViewMode::kChat];
  self.view.backgroundColor = [UIColor clearColor];
}

// Creates a fade effect behind the input plate.
- (void)createInputViewFade {
  if (_inputViewFade) {
    [_inputViewFade removeFromSuperview];
  }

  UIColor* fadeColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  _inputViewFade = [[UIView alloc] init];
  _inputViewFade.userInteractionEnabled = NO;
  _inputViewFade.translatesAutoresizingMaskIntoConstraints = NO;
  _inputViewFade.backgroundColor = fadeColor;

  CAGradientLayer* gradientLayer = [[CAGradientLayer alloc] init];
  gradientLayer.locations = @[ @(0.0), @(1.0) ];
  gradientLayer.colors = @[
    (id)[UIColor clearColor].CGColor,
    (id)[fadeColor colorWithAlphaComponent:0.9].CGColor,
  ];
  gradientLayer.startPoint = CGPointMake(0.5, 0.0);
  gradientLayer.endPoint = CGPointMake(0.5, 1);

  _inputViewFade.layer.mask = gradientLayer;
  _fadeGradient = gradientLayer;
}

// Called right before the keyboard is shown.
- (void)keyboardWillShow:(NSNotification*)notification {
  NSDictionary* userInfo = notification.userInfo;
  NSTimeInterval duration =
      [userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
  UIViewAnimationCurve curve = static_cast<UIViewAnimationCurve>(
      [userInfo[UIKeyboardAnimationCurveUserInfoKey] integerValue]);
  [self.delegate assistantAIMViewController:self
                didShowKeyboardWithDuration:duration
                                      curve:curve];
}

// Called when the keyboard is hidden.
- (void)keyboardDidHide:(NSNotification*)notification {
  _keyboardFrameInWindow = CGRectZero;
  [self updateInputPlateOverlap];
  [self.delegate assistantAIMViewControllerDidHideKeyboard:self];
}

// Adjusts the input plate's bottom margin to account for the keyboard's frame.
- (void)keyboardWillChangeFrame:(NSNotification*)notification {
  if (!self.isViewLoaded || !self.view.window) {
    return;
  }
  NSDictionary* userInfo = notification.userInfo;
  NSValue* rectValue = userInfo[UIKeyboardFrameEndUserInfoKey];
  _keyboardFrameInWindow = rectValue.CGRectValue;

  [self updateInputPlateOverlap];

  NSTimeInterval duration =
      [userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
  UIViewAnimationCurve curve = static_cast<UIViewAnimationCurve>(
      [userInfo[UIKeyboardAnimationCurveUserInfoKey] integerValue]);

  [UIView animateWithDuration:duration
                        delay:0
                      options:curve
                   animations:^{
                     [self.view layoutIfNeeded];
                   }
                   completion:nil];
}

// Updates the input plate's bottom margin to account for the keyboard's frame.
- (void)updateInputPlateOverlap {
  if (CGRectIsEmpty(_keyboardFrameInWindow)) {
    _inputPlateBottomMargin.constant = -kInputPlateMargin;
    return;
  }
  CGRect keyboardFrameInView = [self.view convertRect:_keyboardFrameInWindow
                                             fromView:nil];
  CGFloat overlap =
      CGRectGetMaxY(self.view.bounds) - CGRectGetMinY(keyboardFrameInView);
  CGFloat bottomMargin = MAX(kInputPlateMargin, overlap + kInputPlateMargin);
  _inputPlateBottomMargin.constant = -bottomMargin;
}

// Sets up the web state view.
- (void)setUpWebStateView {
  if (!_webStateView || !self.isViewLoaded) {
    return;
  }

  if (_webStateViewConstraints) {
    [NSLayoutConstraint deactivateConstraints:_webStateViewConstraints];
    _webStateViewConstraints = nil;
  }

  _webStateView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view insertSubview:_webStateView atIndex:0];

  _webStateViewConstraints = @[
    [_webStateView.topAnchor constraintEqualToAnchor:_headerView.bottomAnchor
                                            constant:kTitleVerticalMargin],
    [_webStateView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_webStateView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_webStateView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ];
  [NSLayoutConstraint activateConstraints:_webStateViewConstraints];
}

// Sets up the header view.
- (void)setUpHeader {
  _headerView = [[AssistantAIMHeaderView alloc] init];
  _headerView.actionHandler = _mutator;
  _headerView.translatesAutoresizingMaskIntoConstraints = NO;
  // TODO(crbug.com/492442806): Update title.
  [_headerView setTitle:@"Commuter Bike"];
  _headerView.delegate = self;
  [self.view addSubview:_headerView];

  _headerTopMargin =
      [_headerView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                            constant:kTitleVerticalMargin];
  [NSLayoutConstraint activateConstraints:@[
    _headerTopMargin,
    [_headerView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_headerView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_headerView.heightAnchor constraintEqualToConstant:40],
  ]];
}

#pragma mark - AssistantAIMHeaderViewDelegate

- (void)assistantAIMHeaderViewDidPressClose:
    (AssistantAIMHeaderView*)headerView {
  [self.delegate assistantAIMViewControllerDidTapClose:self];
}

- (void)assistantAIMHeaderViewDidTapBack:(AssistantAIMHeaderView*)headerView {
  [self hideHistory];
}

- (void)assistantAIMHeaderViewDidRequestSRPLogs:
    (AssistantAIMHeaderView*)headerView {
  [self.delegate assistantAIMViewControllerDidRequestSRPLogs:self];
}

#pragma mark - Private

// Sets up a visual indicator when there is a Cobrowse GWS URL override.
- (void)setUpBarricadeTape {
  UIView* tapeView = [[UIView alloc] init];
  tapeView.translatesAutoresizingMaskIntoConstraints = NO;
  tapeView.backgroundColor =
      [UIColor colorWithPatternImage:[self barricadeTapeImage]];
  [self.view addSubview:tapeView];

  [NSLayoutConstraint activateConstraints:@[
    [tapeView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [tapeView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [tapeView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
    [tapeView.heightAnchor constraintEqualToConstant:kBarricadeTapeHeight],
  ]];
}

// Returns a pattern image for the barricade tape.
- (UIImage*)barricadeTapeImage {
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:kBarricadeTapeTileSize];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    [[UIColor colorNamed:kYellow500Color] setFill];
    CGContextFillRect(context.CGContext,
                      CGRectMake(0, 0, kBarricadeTapeTileSize.width,
                                 kBarricadeTapeTileSize.height));

    [[UIColor colorNamed:kSolidBlackColor] setFill];
    UIBezierPath* diagonalStripePath = [UIBezierPath bezierPath];
    [diagonalStripePath moveToPoint:CGPointMake(0, 5)];
    [diagonalStripePath addLineToPoint:CGPointMake(5, 0)];
    [diagonalStripePath addLineToPoint:CGPointMake(10, 0)];
    [diagonalStripePath addLineToPoint:CGPointMake(0, 10)];
    [diagonalStripePath closePath];
    [diagonalStripePath fill];

    UIBezierPath* cornerStripePath = [UIBezierPath bezierPath];
    [cornerStripePath moveToPoint:CGPointMake(5, 10)];
    [cornerStripePath addLineToPoint:CGPointMake(10, 5)];
    [cornerStripePath addLineToPoint:CGPointMake(10, 10)];
    [cornerStripePath closePath];
    [cornerStripePath fill];
  }];
}

@end
