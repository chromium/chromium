// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_presentation_context.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation GeminiContainerViewController {
  // The child view controller wrapping the actual Gemini UI provided by the
  // provider.
  UIViewController* _geminiViewController;
}

- (instancetype)initWithGeminiViewController:
    (UIViewController*)geminiViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _geminiViewController = geminiViewController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(keyboardWillShow:)
                        name:UIKeyboardWillShowNotification
                      object:nil];

  // Prevent double-padding/extra spacing at the bottom when the keyboard
  // is hidden.
  self.view.keyboardLayoutGuide.usesBottomSafeArea = NO;

  UIStackView* containerStack = [[UIStackView alloc] init];
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.axis = UILayoutConstraintAxisVertical;
  containerStack.alignment = UIStackViewAlignmentFill;
  [self.view addSubview:containerStack];

  AddSameConstraintsToSides(
      containerStack, self.view,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  [NSLayoutConstraint activateConstraints:@[
    [containerStack.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor]
  ]];

  if (self.zeroStateViewController) {
    [self addZeroStateToContainer:containerStack];
  }

  if (_geminiViewController) {
    [self addGeminiToContainer:containerStack];
  }
}

#pragma mark - GeminiContainerConsumer

- (void)updateZeroStateVisibility:(BOOL)visible {
  self.zeroStateViewController.view.hidden = !visible;
}

- (void)dismissKeyboard {
  [self.view endEditing:YES];
}

#pragma mark - Private

// Adds the zero-state view controller to `containerStack`.
- (void)addZeroStateToContainer:(UIStackView*)containerStack {
  [self addChildViewController:self.zeroStateViewController];
  [containerStack addArrangedSubview:self.zeroStateViewController.view];

  // Allow the zero-state view to expand into any remaining vertical space
  // and compress first when vertical space is constrained (e.g., when the
  // keyboard is presented).
  [self.zeroStateViewController.view
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisVertical];
  [self.zeroStateViewController.view
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  [self.zeroStateViewController didMoveToParentViewController:self];
}

// Adds the Gemini view controller to `containerStack`.
- (void)addGeminiToContainer:(UIStackView*)containerStack {
  [self addChildViewController:_geminiViewController];
  [containerStack addArrangedSubview:_geminiViewController.view];

  // Keep `_geminiViewController` sized strictly to its intrinsic content
  // height and prevent it from compressing when vertical space is
  // constrained.
  [_geminiViewController.view
      setContentHuggingPriority:UILayoutPriorityDefaultHigh
                        forAxis:UILayoutConstraintAxisVertical];
  [_geminiViewController.view
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];
  [_geminiViewController didMoveToParentViewController:self];
}

// Called right before the keyboard is shown.
- (void)keyboardWillShow:(NSNotification*)notification {
  // Only proceed if the keyboard appeared because the view inside this
  // container or its subviews are the first responder.
  if (!GetFirstResponderSubview(self.view)) {
    return;
  }

  NSDictionary* userInfo = notification.userInfo;
  NSTimeInterval duration =
      [userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
  UIViewAnimationCurve curve = static_cast<UIViewAnimationCurve>(
      [userInfo[UIKeyboardAnimationCurveUserInfoKey] integerValue]);
  [self.delegate geminiContainerViewController:self
                   didShowKeyboardWithDuration:duration
                                         curve:curve];
}

@end
