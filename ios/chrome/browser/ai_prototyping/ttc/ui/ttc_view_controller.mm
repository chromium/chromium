// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_mutator.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_view.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Layout and dimension constants.
constexpr CGFloat kHeaderStackSpacing = 8.0;
constexpr CGFloat kContentStackSpacing = 16.0;
constexpr CGFloat kCardCornerRadius = 12.0;
constexpr CGFloat kCardInternalPadding = 12.0;
constexpr CGFloat kCardItemSpacing = 8.0;

constexpr CGFloat kJournalTextViewHeight = 220.0;
constexpr CGFloat kJournalFontSize = 11.0;
constexpr CGFloat kJournalCornerRadius = 8.0;
constexpr CGFloat kJournalHeaderSpacing = 12.0;

// UI string constants.
NSString* const kHeaderTitleText = @"TTC";
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
  BOOL _isJournalDetailedMode;

  UITextView* _journalTextView;
  UIButton* _journalDetailsButton;
  UIButton* _journalCopyButton;
  UISegmentedControl* _journalFilterControl;
  NSMutableArray<NSString*>* _journalLines;
}

@synthesize feature = _feature;
@synthesize mutator = _mutator;
@synthesize ttcMutator = _ttcMutator;

#pragma mark - Initialization

- (instancetype)initForFeature:(AIPrototypingFeature)feature {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _feature = feature;
    _isJournalDetailedMode = NO;
    _journalLines = [[NSMutableArray alloc] init];
    _voicePlateView = [[TTCVoicePlateView alloc] initWithFrame:CGRectZero];
    _voicePlateView.translatesAutoresizingMaskIntoConstraints = NO;
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

  // Session Journal Card.
  UIView* journalCard = [self createJournalCard];

  // Main Vertical Content Stack.
  UIStackView* mainStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    headerStack, self.voicePlateView, journalCard
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
  ]];

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

#pragma mark - Private UI Helpers

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
