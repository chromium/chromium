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

constexpr CGFloat kTestAudioButtonVerticalInset = 12.0;
constexpr CGFloat kTestAudioButtonHorizontalInset = 16.0;
constexpr CGFloat kMinButtonHeight = 44.0;

// UI string constants.
NSString* const kHeaderTitleText = @"TalkToChrome";
NSString* const kStatusLabelIdleText = @"Tap to start conversation";
NSString* const kStatusLabelConnectingText = @"Connecting to model...";
NSString* const kStatusLabelHandshakingText = @"Handshaking with model...";
NSString* const kStatusLabelListeningText = @"Listening... Speak now";
NSString* const kStatusLabelModelSpeakingText = @"Model speaking...";
NSString* const kStatusLabelErrorText = @"Session error occurred";
NSString* const kEnergyMeterLabelText = @"Microphone Input Level";
NSString* const kDiagnosticsTitleText = @"Developer Diagnostics";
NSString* const kLoopbackSwitchLabelText = @"Mic Loopback (Hear Yourself)";
NSString* const kLoopbackDescriptionText =
    @"Routes mic capture directly to speaker for local hardware testing.";
NSString* const kPlayTestAudioButtonText = @"Play Test Audio (24kHz)";
NSString* const kStopTestAudioButtonText = @"Stop Test Audio";

}  // namespace

@implementation TTCViewController {
  TTCSessionState _currentState;
  BOOL _isTestAudioPlaying;
  BOOL _isLoopbackEnabled;

  UIButton* _micButton;
  UILabel* _statusLabel;
  UIProgressView* _energyLevelMeter;
  UISwitch* _loopbackSwitch;
  UIButton* _testAudioButton;
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
    _isTestAudioPlaying = NO;
    _isLoopbackEnabled = NO;
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

  // Developer Diagnostics Card.
  UIView* diagnosticsCard = [self createDiagnosticsCard];

  // Main Vertical Content Stack.
  UIStackView* mainStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    headerStack, micContainer, meterCard, diagnosticsCard
  ]];
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

  // Re-apply hydrated session and diagnostic states.
  [self setSessionState:_currentState];
  [self setLoopbackEnabled:_isLoopbackEnabled];
  [self setTestAudioPlaying:_isTestAudioPlaying];
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

- (void)setTestAudioPlaying:(BOOL)isPlaying {
  _isTestAudioPlaying = isPlaying;
  if (!_testAudioButton) {
    return;
  }
  UIButtonConfiguration* config = _testAudioButton.configuration;
  if (!config) {
    return;
  }
  if (isPlaying) {
    config.title = kStopTestAudioButtonText;
    config.baseForegroundColor = [UIColor colorNamed:kRedColor];
  } else {
    config.title = kPlayTestAudioButtonText;
    config.baseForegroundColor = [UIColor colorNamed:kBlueColor];
  }
  _testAudioButton.configuration = config;
}

- (void)setLoopbackEnabled:(BOOL)enabled {
  _isLoopbackEnabled = enabled;
  if (_loopbackSwitch) {
    _loopbackSwitch.on = enabled;
  }
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

- (void)loopbackSwitchChanged:(UISwitch*)sender {
  [self.ttcMutator setLoopbackEnabled:sender.isOn];
}

- (void)testAudioButtonTapped:(UIButton*)sender {
  if (_isTestAudioPlaying) {
    [self.ttcMutator stopTestAudio];
  } else {
    [self.ttcMutator playTestAudio];
  }
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

// Creates a card showing developer diagnostic controls: loopback toggle and
// test audio playback.
- (UIView*)createDiagnosticsCard {
  UIView* cardView = [[UIView alloc] init];
  cardView.translatesAutoresizingMaskIntoConstraints = NO;
  cardView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  cardView.layer.cornerRadius = kCardCornerRadius;
  cardView.layer.masksToBounds = YES;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  titleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  titleLabel.text = kDiagnosticsTitleText;

  UILabel* switchLabel = [[UILabel alloc] init];
  switchLabel.translatesAutoresizingMaskIntoConstraints = NO;
  switchLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  switchLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  switchLabel.numberOfLines = 0;
  switchLabel.text = kLoopbackSwitchLabelText;

  _loopbackSwitch = [[UISwitch alloc] init];
  _loopbackSwitch.translatesAutoresizingMaskIntoConstraints = NO;
  _loopbackSwitch.accessibilityLabel = kLoopbackSwitchLabelText;
  _loopbackSwitch.on = _isLoopbackEnabled;
  [_loopbackSwitch addTarget:self
                      action:@selector(loopbackSwitchChanged:)
            forControlEvents:UIControlEventValueChanged];

  UIStackView* switchRow = [[UIStackView alloc]
      initWithArrangedSubviews:@[ switchLabel, _loopbackSwitch ]];
  switchRow.translatesAutoresizingMaskIntoConstraints = NO;
  switchRow.axis = UILayoutConstraintAxisHorizontal;
  switchRow.alignment = UIStackViewAlignmentCenter;
  switchRow.distribution = UIStackViewDistributionEqualSpacing;

  UILabel* descLabel = [[UILabel alloc] init];
  descLabel.translatesAutoresizingMaskIntoConstraints = NO;
  descLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
  descLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  descLabel.numberOfLines = 0;
  descLabel.text = kLoopbackDescriptionText;

  _testAudioButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _testAudioButton.translatesAutoresizingMaskIntoConstraints = NO;
  UIButtonConfiguration* config =
      [UIButtonConfiguration filledButtonConfiguration];
  config.baseBackgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  config.baseForegroundColor = _isTestAudioPlaying
                                   ? [UIColor colorNamed:kRedColor]
                                   : [UIColor colorNamed:kBlueColor];
  config.contentInsets = NSDirectionalEdgeInsetsMake(
      kTestAudioButtonVerticalInset, kTestAudioButtonHorizontalInset,
      kTestAudioButtonVerticalInset, kTestAudioButtonHorizontalInset);
  config.title =
      _isTestAudioPlaying ? kStopTestAudioButtonText : kPlayTestAudioButtonText;
  _testAudioButton.configuration = config;
  [_testAudioButton addTarget:self
                       action:@selector(testAudioButtonTapped:)
             forControlEvents:UIControlEventTouchUpInside];

  UIStackView* cardStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    titleLabel, switchRow, descLabel, _testAudioButton
  ]];
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

    [_testAudioButton.heightAnchor
        constraintGreaterThanOrEqualToConstant:kMinButtonHeight],
  ]];

  return cardView;
}

@end
