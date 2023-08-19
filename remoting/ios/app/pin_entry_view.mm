// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/pin_entry_view.h"

#import <MaterialComponents/MaterialButtons.h>

#include "remoting/base/string_resources.h"
#import "remoting/ios/app/remoting_theme.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kMargin = 6.f;
static const CGFloat kPadding = 8.f;
static const CGFloat kLineSpace = 12.f;

static const int kMinPinLength = 6;

@interface PinEntryView ()<UITextFieldDelegate> {
  UIView* _pairingView;
  UISwitch* _pairingSwitch;
  UILabel* _pairingLabel;
  MDCFloatingButton* _pinButton;
  UITextField* _pinEntry;
}
@end

@implementation PinEntryView

@synthesize delegate = _delegate;
@synthesize supportsPairing = _supportsPairing;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];

    // A view to enlarge the touch area to toggle the |_pairingSwitch|.
    _pairingView = [[UIView alloc] initWithFrame:CGRectZero];
    _pairingView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_pairingView];

    NSString* rememberPinText =
        l10n_util::GetNSString(IDS_REMEMBER_PIN_ON_THIS_DEVICE);
    _pairingSwitch = [[UISwitch alloc] init];
    _pairingSwitch.tintColor = RemotingTheme.pinEntryPairingColor;
    _pairingSwitch.transform = CGAffineTransformMakeScale(0.5, 0.5);
    _pairingSwitch.accessibilityLabel = rememberPinText;
    _pairingSwitch.translatesAutoresizingMaskIntoConstraints = NO;
    [_pairingView addSubview:_pairingSwitch];

    _pairingLabel = [[UILabel alloc] init];
    _pairingLabel.textColor = RemotingTheme.pinEntryPairingColor;
    _pairingLabel.font = [UIFont systemFontOfSize:12.f];
    _pairingLabel.text = rememberPinText;
    _pairingLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [_pairingView addSubview:_pairingLabel];

    // Allow toggling the switch by tapping the pairing view. Note that the
    // gesture recognizer will handle the toggling logic and block tap gesture
    // forwarding towards |_pairingSwitch|.
    UITapGestureRecognizer* tapGestureRecognizer =
        [[UITapGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(didTapPairingView)];
    tapGestureRecognizer.numberOfTapsRequired = 1;
    [_pairingView addGestureRecognizer:tapGestureRecognizer];
    _pairingView.userInteractionEnabled = YES;

    _pinButton =
        [MDCFloatingButton floatingButtonWithShape:MDCFloatingButtonShapeMini];
    [_pinButton
        setImage:[RemotingTheme
                         .arrowIcon imageFlippedForRightToLeftLayoutDirection]
        forState:UIControlStateNormal];
    _pinButton.accessibilityLabel = l10n_util::GetNSString(IDS_CONTINUE_BUTTON);
    [_pinButton setBackgroundColor:RemotingTheme.buttonBackgroundColor
                          forState:UIControlStateNormal];
    [_pinButton setDisabledAlpha:0.7];
    [_pinButton addTarget:self
                   action:@selector(didTapPinEntry:)
         forControlEvents:UIControlEventTouchUpInside];
    _pinButton.translatesAutoresizingMaskIntoConstraints = NO;
    _pinButton.enabled = NO;
    _pinButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_pinButton];

    _pinEntry = [[UITextField alloc] init];
    _pinEntry.textColor = RemotingTheme.pinEntryTextColor;
    _pinEntry.secureTextEntry = YES;
    _pinEntry.keyboardType = UIKeyboardTypeNumberPad;
    _pinEntry.attributedPlaceholder = [[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(IDS_ENTER_PIN)
            attributes:@{
              NSForegroundColorAttributeName :
                  RemotingTheme.pinEntryPlaceholderColor
            }];
    _pinEntry.translatesAutoresizingMaskIntoConstraints = NO;
    _pinEntry.delegate = self;
    [self addSubview:_pinEntry];

    [self initializeLayoutConstraints];

    _supportsPairing = YES;

    self.accessibilityElements = @[ _pinEntry, _pairingSwitch, _pinButton ];
  }
  return self;
}

- (void)initializeLayoutConstraints {
  NSDictionary* views =
      NSDictionaryOfVariableBindings(_pairingView, _pinButton, _pinEntry);
  // Metrics to use in visual format strings.
  NSDictionary* layoutMetrics = @{
    @"padding" : @(kPadding),
    @"lineSpace" : @(kLineSpace),
  };

  [self addConstraints:
            [NSLayoutConstraint
                constraintsWithVisualFormat:
                    @"H:|-[_pinEntry]-(padding)-[_pinButton]-|"
                                    options:NSLayoutFormatAlignAllCenterY
                                    metrics:layoutMetrics
                                      views:views]];

  [self addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:
                               @"V:|-[_pinButton]-(lineSpace)-[_pairingView]"
                                               options:0
                                               metrics:layoutMetrics
                                                 views:views]];

  [NSLayoutConstraint activateConstraints:@[
    [_pairingSwitch.centerYAnchor
        constraintEqualToAnchor:_pairingView.centerYAnchor],
    [_pairingLabel.centerYAnchor
        constraintEqualToAnchor:_pairingView.centerYAnchor],
    [_pairingLabel.leadingAnchor
        constraintEqualToAnchor:_pairingSwitch.trailingAnchor
                       constant:kPadding],

    [_pairingView.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [_pairingView.heightAnchor
        constraintEqualToAnchor:_pairingLabel.heightAnchor
                       constant:2 * kMargin],
    [_pairingView.leadingAnchor
        constraintEqualToAnchor:_pairingSwitch.leadingAnchor
                       constant:-kMargin],
    [_pairingView.trailingAnchor
        constraintEqualToAnchor:_pairingLabel.trailingAnchor
                       constant:kMargin],
  ]];

  [self setNeedsUpdateConstraints];
}

#pragma mark - UIView

- (BOOL)canBecomeFirstResponder {
  return [_pinEntry canBecomeFirstResponder];
}

- (BOOL)becomeFirstResponder {
  return [_pinEntry becomeFirstResponder];
}

- (BOOL)endEditing:(BOOL)force {
  return [_pinEntry endEditing:force];
}

#pragma mark - Properties

- (void)setSupportsPairing:(BOOL)supportsPairing {
  _supportsPairing = supportsPairing;
  _pairingSwitch.hidden = !_supportsPairing;
  _pairingSwitch.isAccessibilityElement = _supportsPairing;
  [_pairingSwitch setOn:NO animated:NO];
  _pairingLabel.hidden = !_supportsPairing;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  if (textField == _pinEntry) {
    NSUInteger length = _pinEntry.text.length - range.length + string.length;
    _pinButton.enabled = length >= kMinPinLength;
  }
  return YES;
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  if ([_pinButton isEnabled]) {
    [self didTapPinEntry:textField];
    return YES;
  }
  return NO;
}

#pragma mark - Public

- (void)clearPinEntry {
  _pinEntry.text = @"";
  _pinButton.enabled = NO;
}

#pragma mark - Private

- (void)didTapPinEntry:(id)sender {
  [_delegate didProvidePin:_pinEntry.text createPairing:_pairingSwitch.isOn];
  [_pinEntry endEditing:YES];
}

- (void)didTapPairingView {
  [_pairingSwitch setOn:!_pairingSwitch.isOn animated:YES];
}

@end
