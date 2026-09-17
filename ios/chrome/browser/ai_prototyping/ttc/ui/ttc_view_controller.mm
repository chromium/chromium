// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_mutator.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Layout and dimension constants.
constexpr CGFloat kMicButtonSize = 88.0;
constexpr CGFloat kMicSymbolPointSize = 40.0;
constexpr CGFloat kHeaderStackSpacing = 8.0;
constexpr CGFloat kContentStackSpacing = 16.0;
constexpr CGFloat kStatusLabelSpacing = 10.0;
constexpr CGFloat kCardCornerRadius = 12.0;
constexpr CGFloat kCardInternalPadding = 12.0;
constexpr CGFloat kCardItemSpacing = 8.0;

// UI string constants.
NSString* const kHeaderTitleText = @"TalkToChrome";
NSString* const kStatusLabelIdleText = @"Tap to start conversation";
NSString* const kStatusLabelConnectingText = @"Connecting to model...";
NSString* const kStatusLabelHandshakingText = @"Handshaking with model...";
NSString* const kStatusLabelListeningText = @"Listening... Speak now";
NSString* const kStatusLabelModelSpeakingText = @"Model speaking...";
NSString* const kStatusLabelErrorText = @"Session error occurred";
NSString* const kEnergyMeterLabelText = @"Microphone Input Level";

}  // namespace

@implementation TTCViewController {
  TTCSessionState _currentState;

  UIButton* _micButton;
  UILabel* _statusLabel;
  UIProgressView* _energyLevelMeter;
}

@synthesize feature = _feature;
@synthesize mutator = _mutator;
@synthesize ttcMutator = _ttcMutator;

#pragma mark - Initialization

- (instancetype)initForFeature:(AIPrototypingFeature)feature {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _feature = feature;
    _currentState = TTCSessionState::kIdle;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.ttcMutator viewWillAppear];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent],
  ];

  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];

  // Header Title.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.text = kHeaderTitleText;

  UIStackView* headerStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ titleLabel ]];
  headerStack.translatesAutoresizingMaskIntoConstraints = NO;
  headerStack.axis = UILayoutConstraintAxisVertical;
  headerStack.spacing = kHeaderStackSpacing;

  // Mic Button.
  _micButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _micButton.translatesAutoresizingMaskIntoConstraints = NO;
  _micButton.backgroundColor = [UIColor colorNamed:kBlueColor];
  _micButton.tintColor = [UIColor whiteColor];
  _micButton.layer.cornerRadius = kMicButtonSize / 2.0;
  _micButton.layer.masksToBounds = NO;
  _micButton.layer.shadowColor = [[UIColor blackColor] CGColor];
  _micButton.layer.shadowOffset = CGSizeMake(0.0, 4.0);
  _micButton.layer.shadowOpacity = 0.2;
  _micButton.layer.shadowRadius = 8.0;

  UIImage* micImage =
      SymbolWithPointSize(SymbolMicrophoneFill, kMicSymbolPointSize);
  [_micButton setImage:micImage forState:UIControlStateNormal];
  [_micButton addTarget:self
                 action:@selector(micButtonTapped:)
       forControlEvents:UIControlEventTouchUpInside];

  // Status Label.
  _statusLabel = [[UILabel alloc] init];
  _statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _statusLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  _statusLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _statusLabel.textAlignment = NSTextAlignmentCenter;
  _statusLabel.numberOfLines = 0;
  _statusLabel.text = kStatusLabelIdleText;

  UIStackView* micContainer = [[UIStackView alloc]
      initWithArrangedSubviews:@[ _micButton, _statusLabel ]];
  micContainer.translatesAutoresizingMaskIntoConstraints = NO;
  micContainer.axis = UILayoutConstraintAxisVertical;
  micContainer.alignment = UIStackViewAlignmentCenter;
  micContainer.spacing = kStatusLabelSpacing;

  // Microphone Input Level Card.
  UIView* meterCard = [self createMeterCard];

  // Main Vertical Content Stack.
  UIStackView* mainStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ headerStack, micContainer, meterCard ]];
  mainStack.translatesAutoresizingMaskIntoConstraints = NO;
  mainStack.axis = UILayoutConstraintAxisVertical;
  mainStack.spacing = kContentStackSpacing;

  // Wrap in a scroll view to accommodate presentation across different sheet
  // detents and device sizes.
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];
  [scrollView addSubview:mainStack];

  [NSLayoutConstraint activateConstraints:@[
    [scrollView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [scrollView.bottomAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],
    [scrollView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],

    [mainStack.topAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor
                       constant:kMainStackTopInset],
    [mainStack.bottomAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor
                       constant:-kMainStackTopInset],
    [mainStack.leadingAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.leadingAnchor
                       constant:kHorizontalInset],
    [mainStack.trailingAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.trailingAnchor
                       constant:-kHorizontalInset],

    [_micButton.widthAnchor constraintEqualToConstant:kMicButtonSize],
    [_micButton.heightAnchor constraintEqualToConstant:kMicButtonSize],
  ]];

  // Re-apply current session state.
  [self setSessionState:_currentState];
}

#pragma mark - AIPrototypingViewControllerProtocol

- (void)enableSubmitButtons {
  // No-op for TTC audio session.
}

- (void)updateResponseField:(NSString*)response {
  // No-op for TTC audio session.
}

#pragma mark - TTCConsumer

- (void)setSessionState:(TTCSessionState)state {
  _currentState = state;
  switch (state) {
    case TTCSessionState::kIdle:
      _micButton.backgroundColor = [UIColor colorNamed:kBlueColor];
      _statusLabel.text = kStatusLabelIdleText;
      _statusLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
      [_energyLevelMeter setProgress:0.0 animated:NO];
      break;
    case TTCSessionState::kConnecting:
      _micButton.backgroundColor = [UIColor colorNamed:kOrange500Color];
      _statusLabel.text = kStatusLabelConnectingText;
      _statusLabel.textColor = [UIColor colorNamed:kOrange500Color];
      break;
    case TTCSessionState::kHandshaking:
      _micButton.backgroundColor = [UIColor colorNamed:kOrange500Color];
      _statusLabel.text = kStatusLabelHandshakingText;
      _statusLabel.textColor = [UIColor colorNamed:kOrange500Color];
      break;
    case TTCSessionState::kListening:
      _micButton.backgroundColor = [UIColor colorNamed:kRedColor];
      _statusLabel.text = kStatusLabelListeningText;
      _statusLabel.textColor = [UIColor colorNamed:kRedColor];
      break;
    case TTCSessionState::kModelSpeaking:
      _micButton.backgroundColor = [UIColor colorNamed:kGreenColor];
      _statusLabel.text = kStatusLabelModelSpeakingText;
      _statusLabel.textColor = [UIColor colorNamed:kGreenColor];
      break;
    case TTCSessionState::kError:
      _micButton.backgroundColor = [UIColor colorNamed:kBlueColor];
      _statusLabel.text = kStatusLabelErrorText;
      _statusLabel.textColor = [UIColor colorNamed:kRedColor];
      [_energyLevelMeter setProgress:0.0 animated:NO];
      break;
  }
}

- (void)setMicEnergyLevel:(float)rms {
  [_energyLevelMeter setProgress:rms animated:YES];
}

- (void)didEncounterError:(NSString*)errorMessage {
  _statusLabel.text = errorMessage ?: kStatusLabelErrorText;
  _statusLabel.textColor = [UIColor colorNamed:kRedColor];
}

#pragma mark - Actions

- (void)micButtonTapped:(UIButton*)sender {
  if (_currentState != TTCSessionState::kIdle &&
      _currentState != TTCSessionState::kError) {
    [self.ttcMutator stopSession];
    return;
  }

  [self.ttcMutator startSession];
}

#pragma mark - Private UI Helpers

// Creates a card showing the microphone input energy progress meter.
- (UIView*)createMeterCard {
  UIView* cardView = [[UIView alloc] init];
  cardView.translatesAutoresizingMaskIntoConstraints = NO;
  cardView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  cardView.layer.cornerRadius = kCardCornerRadius;
  cardView.layer.masksToBounds = YES;

  UILabel* meterLabel = [[UILabel alloc] init];
  meterLabel.translatesAutoresizingMaskIntoConstraints = NO;
  meterLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  meterLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  meterLabel.text = kEnergyMeterLabelText;

  _energyLevelMeter = [[UIProgressView alloc]
      initWithProgressViewStyle:UIProgressViewStyleDefault];
  _energyLevelMeter.translatesAutoresizingMaskIntoConstraints = NO;
  _energyLevelMeter.progressTintColor = [UIColor colorNamed:kBlueColor];
  _energyLevelMeter.trackTintColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  _energyLevelMeter.progress = 0.0;

  UIStackView* cardStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ meterLabel, _energyLevelMeter ]];
  cardStack.translatesAutoresizingMaskIntoConstraints = NO;
  cardStack.axis = UILayoutConstraintAxisVertical;
  cardStack.spacing = kCardItemSpacing;

  [cardView addSubview:cardStack];

  [NSLayoutConstraint activateConstraints:@[
    [cardStack.topAnchor constraintEqualToAnchor:cardView.topAnchor
                                        constant:kCardInternalPadding],
    [cardStack.bottomAnchor constraintEqualToAnchor:cardView.bottomAnchor
                                           constant:-kCardInternalPadding],
    [cardStack.leadingAnchor constraintEqualToAnchor:cardView.leadingAnchor
                                            constant:kCardInternalPadding],
    [cardStack.trailingAnchor constraintEqualToAnchor:cardView.trailingAnchor
                                             constant:-kCardInternalPadding],
  ]];

  return cardView;
}

@end
