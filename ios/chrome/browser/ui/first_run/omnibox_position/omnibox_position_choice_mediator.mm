// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/metrics.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_consumer.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_util.h"

@interface OmniboxPositionChoiceMediator ()

/// The selected omnibox position.
@property(nonatomic, assign) ToolbarType selectedPosition;

@end

@implementation OmniboxPositionChoiceMediator {
  /// Whether the screen is being shown in the FRE.
  BOOL _isFirstRun;
}

- (instancetype)initWithFirstRun:(BOOL)isFirstRun {
  self = [super init];
  if (self) {
    _selectedPosition = DefaultSelectedOmniboxPosition();
    _isFirstRun = isFirstRun;
  }
  return self;
}

- (void)saveSelectedPosition {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kBottomOmnibox, self.selectedPosition == ToolbarType::kSecondary);
  RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kPositionValidated,
                    _isFirstRun);
  RecordSelectedPosition(
      self.selectedPosition,
      self.selectedPosition == DefaultSelectedOmniboxPosition(), _isFirstRun,
      self.deviceSwitcherResultDispatcher);
}

- (void)discardSelectedPosition {
  CHECK(!_isFirstRun);  // Discard is not available on first run.
  RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kPositionDiscarded,
                    _isFirstRun);
}

- (void)skipSelection {
  CHECK(_isFirstRun);
    const BOOL defaultPositionIsBottom =
        DefaultSelectedOmniboxPosition() == ToolbarType::kSecondary;
    PrefService* localState = GetApplicationContext()->GetLocalState();
    localState->SetBoolean(prefs::kBottomOmniboxByDefault,
                           defaultPositionIsBottom);
    localState->SetDefaultPrefValue(prefs::kBottomOmnibox,
                                    base::Value(defaultPositionIsBottom));
    RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kScreenSkipped,
                      _isFirstRun);
}

#pragma mark - Setters

- (void)setConsumer:(id<OmniboxPositionChoiceConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setSelectedToolbarForOmnibox:self.selectedPosition];
}

- (void)setSelectedPosition:(ToolbarType)position {
  _selectedPosition = position;
  [self.consumer setSelectedToolbarForOmnibox:position];
}

#pragma mark - OmniboxPositionChoiceMutator

- (void)selectTopOmnibox {
  self.selectedPosition = ToolbarType::kPrimary;
  RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kTopOptionSelected,
                    _isFirstRun);
}

- (void)selectBottomOmnibox {
  self.selectedPosition = ToolbarType::kSecondary;
  RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kBottomOptionSelected,
                    _isFirstRun);
}

@end
