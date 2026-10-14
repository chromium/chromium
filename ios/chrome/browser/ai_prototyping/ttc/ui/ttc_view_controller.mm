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
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

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
constexpr CGFloat kMicButtonShadowOffsetY = 4.0;
constexpr float kMicButtonShadowOpacity = 0.2f;
constexpr CGFloat kMicButtonShadowRadius = 8.0;
constexpr CGFloat kJournalTextViewHeight = 220.0;
constexpr CGFloat kJournalFontSize = 11.0;
constexpr CGFloat kJournalCornerRadius = 8.0;
constexpr CGFloat kJournalHeaderSpacing = 12.0;

// UI string constants.
NSString* const kHeaderTitleText = @"TTC";
NSString* const kStatusLabelIdleText = @"Tap to start conversation";
NSString* const kStatusLabelConnectingText = @"Connecting to server...";
NSString* const kStatusLabelHandshakingText = @"Handshaking with server...";
NSString* const kStatusLabelListeningText = @"Listening... Speak now";
NSString* const kStatusLabelModelSpeakingText = @"Speaking...";
NSString* const kStatusLabelErrorText = @"Session error occurred";
NSString* const kEnergyMeterLabelText = @"Microphone Input Level";
NSString* const kJournalTitleText = @"Session Journal";
NSString* const kJournalDetailsButtonText = @"Details";
NSString* const kJournalSimpleButtonText = @"Simple";
NSString* const kJournalCopyButtonText = @"Copy";
NSString* const kJournalClearButtonText = @"Clear";
NSString* const kJournalFilterAllText = @"All";
NSString* const kJournalFilterSessionText = @"Session";
NSString* const kJournalFilterConvText = @"Conv";
NSString* const kJournalFilterBackendText = @"Backend";
NSString* const kJournalFilterAudioText = @"Audio";
NSString* const kJournalFilterActuationText = @"Actuation";
NSString* const kDiagnosticsTitleText = @"Developer Diagnostics";
NSString* const kLoopbackSwitchLabelText = @"Mic Loopback (Hear Yourself)";
NSString* const kLoopbackDescriptionText =
    @"Routes mic capture directly to speaker for local hardware testing.";
NSString* const kPlayTestAudioButtonText = @"Play Test Audio (24kHz)";
NSString* const kStopTestAudioButtonText = @"Stop Test Audio";

// Maps a segmented control index to the corresponding `TTCJournalFilter`.
TTCJournalFilter JournalFilterForSegmentIndex(NSInteger index) {
  switch (static_cast<TTCJournalFilter>(index)) {
    case TTCJournalFilter::kAll:
    case TTCJournalFilter::kSession:
    case TTCJournalFilter::kConversation:
    case TTCJournalFilter::kBackend:
    case TTCJournalFilter::kAudio:
    case TTCJournalFilter::kActuation:
      return static_cast<TTCJournalFilter>(index);
  }
  return TTCJournalFilter::kAll;
}

}  // namespace

@implementation TTCViewController {
  TTCSessionUIState _currentState;
  BOOL _isTestAudioPlaying;
  BOOL _isLoopbackEnabled;
  BOOL _isJournalDetailedMode;

  UIButton* _micButton;
  UILabel* _statusLabel;
  UIProgressView* _energyLevelMeter;
  UITextView* _journalTextView;
  UIButton* _journalDetailsButton;
  UIButton* _journalCopyButton;
  UISegmentedControl* _journalFilterControl;
  NSMutableArray<NSString*>* _journalLines;
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
    _currentState = TTCSessionUIState::kIdle;
    _isTestAudioPlaying = NO;
    _isLoopbackEnabled = NO;
    _isJournalDetailedMode = NO;
    _journalLines = [[NSMutableArray alloc] init];
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
  _micButton.layer.shadowOffset = CGSizeMake(0.0, kMicButtonShadowOffsetY);
  _micButton.layer.shadowOpacity = kMicButtonShadowOpacity;
  _micButton.layer.shadowRadius = kMicButtonShadowRadius;

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

  // Session Journal Card.
  UIView* journalCard = [self createJournalCard];

  // Developer Diagnostics Card (hidden as prototype audio diagnostics are
  // disconnected from the live service session).
  UIView* diagnosticsCard = [self createDiagnosticsCard];
  diagnosticsCard.hidden = YES;

  // Main Vertical Content Stack.
  UIStackView* mainStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    headerStack, micContainer, meterCard, journalCard, diagnosticsCard
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
  [self refreshJournalTextView];
}

#pragma mark - AIPrototypingViewControllerProtocol

- (void)enableSubmitButtons {
  // No-op for TTC audio session.
}

- (void)updateResponseField:(NSString*)response {
  // No-op for TTC audio session.
}

#pragma mark - TTCConsumer

- (void)setSessionState:(TTCSessionUIState)state {
  _currentState = state;
  switch (state) {
    case TTCSessionUIState::kIdle:
      _micButton.backgroundColor = [UIColor colorNamed:kBlueColor];
      _statusLabel.text = kStatusLabelIdleText;
      _statusLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
      [_energyLevelMeter setProgress:0.0 animated:NO];
      break;
    case TTCSessionUIState::kConnecting:
      _micButton.backgroundColor = [UIColor colorNamed:kOrange500Color];
      _statusLabel.text = kStatusLabelConnectingText;
      _statusLabel.textColor = [UIColor colorNamed:kOrange500Color];
      break;
    case TTCSessionUIState::kHandshaking:
      _micButton.backgroundColor = [UIColor colorNamed:kOrange500Color];
      _statusLabel.text = kStatusLabelHandshakingText;
      _statusLabel.textColor = [UIColor colorNamed:kOrange500Color];
      break;
    case TTCSessionUIState::kListening:
      _micButton.backgroundColor = [UIColor colorNamed:kRedColor];
      _statusLabel.text = kStatusLabelListeningText;
      _statusLabel.textColor = [UIColor colorNamed:kRedColor];
      break;
    case TTCSessionUIState::kModelSpeaking:
      _micButton.backgroundColor = [UIColor colorNamed:kGreenColor];
      _statusLabel.text = kStatusLabelModelSpeakingText;
      _statusLabel.textColor = [UIColor colorNamed:kGreenColor];
      break;
    case TTCSessionUIState::kError:
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

- (void)setJournalLines:(NSArray<NSString*>*)lines {
  [_journalLines removeAllObjects];
  if (lines) {
    [_journalLines addObjectsFromArray:lines];
  }
  [self refreshJournalTextView];
}

- (void)appendJournalLine:(NSString*)line {
  if (!line) {
    return;
  }
  [_journalLines addObject:line];
  [self refreshJournalTextView];
}

#pragma mark - Actions

- (void)micButtonTapped:(UIButton*)sender {
  if (_currentState != TTCSessionUIState::kIdle &&
      _currentState != TTCSessionUIState::kError) {
    [self.ttcMutator stopSession];
    return;
  }

  [self.ttcMutator startSession];
}

- (void)detailsJournalButtonTapped:(UIButton*)sender {
  _isJournalDetailedMode = !_isJournalDetailedMode;
  [_journalDetailsButton
      setTitle:(_isJournalDetailedMode ? kJournalSimpleButtonText
                                       : kJournalDetailsButtonText)
      forState:UIControlStateNormal];
  [self.ttcMutator setJournalDetailedMode:_isJournalDetailedMode];
}

- (void)copyJournalButtonTapped:(UIButton*)sender {
  if (_journalTextView.text.length > 0) {
    UIPasteboard.generalPasteboard.string = _journalTextView.text;
  }
}

- (void)clearJournalButtonTapped:(UIButton*)sender {
  [self.ttcMutator clearJournal];
}

- (void)journalFilterChanged:(UISegmentedControl*)sender {
  [self.ttcMutator setJournalComponentFilter:JournalFilterForSegmentIndex(
                                                 sender.selectedSegmentIndex)];
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

// Creates a card displaying the live session journal with component filter,
// simplified/detailed toggle, copy, and clear controls.
- (UIView*)createJournalCard {
  UIView* cardView = [[UIView alloc] init];
  cardView.translatesAutoresizingMaskIntoConstraints = NO;
  cardView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  cardView.layer.cornerRadius = kCardCornerRadius;
  cardView.layer.masksToBounds = YES;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  titleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  titleLabel.text = kJournalTitleText;

  _journalDetailsButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _journalDetailsButton.translatesAutoresizingMaskIntoConstraints = NO;
  [_journalDetailsButton
      setTitle:(_isJournalDetailedMode ? kJournalSimpleButtonText
                                       : kJournalDetailsButtonText)
      forState:UIControlStateNormal];
  _journalDetailsButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  [_journalDetailsButton addTarget:self
                            action:@selector(detailsJournalButtonTapped:)
                  forControlEvents:UIControlEventTouchUpInside];

  _journalCopyButton = [UIButton buttonWithType:UIButtonTypeSystem];
  _journalCopyButton.translatesAutoresizingMaskIntoConstraints = NO;
  _journalCopyButton.enabled = _journalLines.count > 0;
  [_journalCopyButton setTitle:kJournalCopyButtonText
                      forState:UIControlStateNormal];
  _journalCopyButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  [_journalCopyButton addTarget:self
                         action:@selector(copyJournalButtonTapped:)
               forControlEvents:UIControlEventTouchUpInside];

  UIButton* clearButton = [UIButton buttonWithType:UIButtonTypeSystem];
  clearButton.translatesAutoresizingMaskIntoConstraints = NO;
  [clearButton setTitle:kJournalClearButtonText forState:UIControlStateNormal];
  clearButton.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
  [clearButton setTitleColor:[UIColor colorNamed:kRedColor]
                    forState:UIControlStateNormal];
  [clearButton addTarget:self
                  action:@selector(clearJournalButtonTapped:)
        forControlEvents:UIControlEventTouchUpInside];

  UIStackView* actionButtonsStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        _journalDetailsButton, _journalCopyButton, clearButton
      ]];
  actionButtonsStack.translatesAutoresizingMaskIntoConstraints = NO;
  actionButtonsStack.axis = UILayoutConstraintAxisHorizontal;
  actionButtonsStack.spacing = kJournalHeaderSpacing;

  UIStackView* headerRow = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, actionButtonsStack ]];
  headerRow.translatesAutoresizingMaskIntoConstraints = NO;
  headerRow.axis = UILayoutConstraintAxisHorizontal;
  headerRow.alignment = UIStackViewAlignmentCenter;
  headerRow.distribution = UIStackViewDistributionEqualSpacing;

  _journalFilterControl = [[UISegmentedControl alloc] initWithItems:@[
    kJournalFilterAllText, kJournalFilterSessionText, kJournalFilterConvText,
    kJournalFilterBackendText, kJournalFilterAudioText,
    kJournalFilterActuationText
  ]];
  _journalFilterControl.translatesAutoresizingMaskIntoConstraints = NO;
  _journalFilterControl.apportionsSegmentWidthsByContent = YES;
  _journalFilterControl.selectedSegmentIndex = 0;
  [_journalFilterControl addTarget:self
                            action:@selector(journalFilterChanged:)
                  forControlEvents:UIControlEventValueChanged];

  _journalTextView = [[UITextView alloc] init];
  _journalTextView.translatesAutoresizingMaskIntoConstraints = NO;
  _journalTextView.editable = NO;
  _journalTextView.selectable = YES;
  _journalTextView.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  _journalTextView.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _journalTextView.font =
      [UIFont monospacedSystemFontOfSize:kJournalFontSize
                                  weight:UIFontWeightRegular];
  _journalTextView.layer.cornerRadius = kJournalCornerRadius;
  _journalTextView.layer.masksToBounds = YES;

  UIStackView* cardStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    headerRow, _journalFilterControl, _journalTextView
  ]];
  cardStack.translatesAutoresizingMaskIntoConstraints = NO;
  cardStack.axis = UILayoutConstraintAxisVertical;
  cardStack.spacing = kCardItemSpacing;

  [cardView addSubview:cardStack];

  AddSameConstraintsWithInset(cardStack, cardView, kCardInternalPadding);
  [_journalTextView.heightAnchor
      constraintEqualToConstant:kJournalTextViewHeight]
      .active = YES;

  return cardView;
}

// Updates `_journalTextView` with the full contents of `_journalLines`.
- (void)refreshJournalTextView {
  if (!_journalTextView) {
    return;
  }
  _journalTextView.text = [_journalLines componentsJoinedByString:@"\n"];
  _journalCopyButton.enabled = _journalLines.count > 0;
  [self scrollJournalToBottom];
}

// Scrolls `_journalTextView` to the bottom when non-empty.
- (void)scrollJournalToBottom {
  if (!_journalTextView || _journalTextView.text.length == 0) {
    return;
  }
  NSRange bottomRange = NSMakeRange(_journalTextView.text.length - 1, 1);
  [_journalTextView scrollRangeToVisible:bottomRange];
}

@end
