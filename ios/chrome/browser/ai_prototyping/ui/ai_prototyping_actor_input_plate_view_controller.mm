// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_actor_input_plate_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actor_input_plate_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/chrome_button.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kLayoutSpacing = 16.0;

NSString* const kNothingSentText = @"Nothing sent yet.";
NSString* const kLastSentFormat = @"Last sent: \"%@\"";
NSString* const kReplyButtonTitle = @"Reply";

// Reply button / input plate morph animation.
const NSTimeInterval kMorphAnimationDuration = 0.35;
const CGFloat kMorphSpringDamping = 0.85;
const CGFloat kMorphShrunkScale = 0.96;

// Shows or hides `view` as one side of the reply morph.
void SetMorphVisible(UIView* view, BOOL visible) {
  view.alpha = visible ? 1.0 : 0.0;
  view.transform = visible ? CGAffineTransformIdentity
                           : CGAffineTransformMakeScale(kMorphShrunkScale,
                                                        kMorphShrunkScale);
  view.accessibilityElementsHidden = !visible;
}

}  // namespace

@interface AIPrototypingActorInputPlateViewController () <
    ActorInputPlateViewDelegate>
@end

@implementation AIPrototypingActorInputPlateViewController {
  UIScrollView* _scrollView;
  UIStackView* _mainStack;

  ActorInputPlateView* _inputPlate;
  UIView* _replyContainer;
  ChromeButton* _replyButton;
  BOOL _replyExpanded;

  UILabel* _lastSentLabel;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Actor Input Plate";
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  _scrollView.keyboardDismissMode = UIScrollViewKeyboardDismissModeInteractive;
  [self.view addSubview:_scrollView];

  UITapGestureRecognizer* tap = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(dismissKeyboard)];
  // Lets taps reach the controls inside the scroll view.
  tap.cancelsTouchesInView = NO;
  [_scrollView addGestureRecognizer:tap];

  _mainStack = [[UIStackView alloc] init];
  _mainStack.axis = UILayoutConstraintAxisVertical;
  _mainStack.spacing = kLayoutSpacing;
  _mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:_mainStack];

  [self createReplyContainer];
  [self createLastSentLabel];
  [self setupConstraints];
}

#pragma mark - ActorInputPlateViewDelegate

- (void)actorInputPlateView:(ActorInputPlateView*)inputPlateView
                didSendText:(NSString*)text {
  _lastSentLabel.text = [NSString stringWithFormat:kLastSentFormat, text];
  [self setReplyExpanded:NO];
}

#pragma mark - Actions

- (void)dismissKeyboard {
  [self.view endEditing:YES];
  // Backs out of an empty reply; a draft keeps the plate visible.
  if (_inputPlate.text.length == 0) {
    [self setReplyExpanded:NO];
  }
}

- (void)replyButtonTapped {
  [self setReplyExpanded:YES];
}

#pragma mark - Private

// Creates the bottom container for the reply morph. The button sets its height,
// which the plate fills so both sides of the morph match.
- (void)createReplyContainer {
  _replyContainer = [[UIView alloc] init];
  _replyContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_replyContainer];

  _replyButton = [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  _replyButton.translatesAutoresizingMaskIntoConstraints = NO;
  _replyButton.title = kReplyButtonTitle;
  [_replyButton addTarget:self
                   action:@selector(replyButtonTapped)
         forControlEvents:UIControlEventTouchUpInside];
  [_replyContainer addSubview:_replyButton];
  AddSameConstraints(_replyButton, _replyContainer);

  _inputPlate = [[ActorInputPlateView alloc] init];
  _inputPlate.translatesAutoresizingMaskIntoConstraints = NO;
  _inputPlate.delegate = self;
  [_replyContainer addSubview:_inputPlate];
  AddSameConstraints(_inputPlate, _replyContainer);
  [self applyReplyExpanded:NO];
}

// Cross-fades the "Reply" button into the input plate, or back. Focus moves
// with it so the keyboard animates alongside.
- (void)setReplyExpanded:(BOOL)expanded {
  if (_replyExpanded == expanded) {
    return;
  }
  _replyExpanded = expanded;

  // Runs before the animation so the keyboard moves the container in the same
  // transaction.
  if (expanded) {
    [_inputPlate becomeFirstResponder];
  } else {
    [self.view endEditing:YES];
  }

  [UIView animateWithDuration:kMorphAnimationDuration
                        delay:0.0
       usingSpringWithDamping:kMorphSpringDamping
        initialSpringVelocity:0.0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     [self applyReplyExpanded:expanded];
                   }
                   completion:nil];
}

// Shows the input plate when `expanded`, otherwise the "Reply" button.
- (void)applyReplyExpanded:(BOOL)expanded {
  SetMorphVisible(_inputPlate, expanded);
  SetMorphVisible(_replyButton, !expanded);
}

// Pins the scrollable controls above the reply container, which rides the
// keyboard.
- (void)setupConstraints {
  UILayoutGuide* safeArea = self.view.safeAreaLayoutGuide;
  AddSameConstraintsToSides(
      _scrollView, safeArea,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSidesWithInsets(
      _replyContainer, safeArea, LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(0, kLayoutSpacing, 0, kLayoutSpacing));
  [NSLayoutConstraint activateConstraints:@[
    [_scrollView.bottomAnchor constraintEqualToAnchor:_replyContainer.topAnchor
                                             constant:-kLayoutSpacing],
    // Rides above the keyboard, matching the intended placement.
    [_replyContainer.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                       constant:-kLayoutSpacing],
  ]];

  AddSameConstraintsWithInset(_mainStack, _scrollView.contentLayoutGuide,
                              kLayoutSpacing);
  [_mainStack.widthAnchor
      constraintEqualToAnchor:_scrollView.frameLayoutGuide.widthAnchor
                     constant:-2.0 * kLayoutSpacing]
      .active = YES;
}

// Creates the label echoing the last text received from the delegate.
- (void)createLastSentLabel {
  _lastSentLabel = [self labelWithText:kNothingSentText
                             textStyle:UIFontTextStyleBody
                             colorName:kTextPrimaryColor];
  _lastSentLabel.numberOfLines = 0;
  [_mainStack addArrangedSubview:_lastSentLabel];
}

// Returns a Dynamic Type label using `textStyle` and the named color.
- (UILabel*)labelWithText:(NSString*)text
                textStyle:(UIFontTextStyle)textStyle
                colorName:(NSString*)colorName {
  UILabel* label = [[UILabel alloc] init];
  label.text = text;
  label.font = [UIFont preferredFontForTextStyle:textStyle];
  label.textColor = [UIColor colorNamed:colorName];
  return label;
}

@end
