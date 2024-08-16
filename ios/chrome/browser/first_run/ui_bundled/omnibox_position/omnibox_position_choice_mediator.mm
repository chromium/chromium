// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/omnibox_position_choice_consumer.h"

@interface OmniboxPositionChoiceMediator ()

/// The selected omnibox position.
@property(nonatomic, assign) ToolbarType selectedPosition;

@end

@implementation OmniboxPositionChoiceMediator

- (instancetype)init {
  self = [super init];
  if (self) {
    _selectedPosition = ToolbarType::kPrimary;
  }
  return self;
}

- (void)saveSelectedPosition {
  GetApplicationContext()->GetLocalState()->SetBoolean(
      prefs::kBottomOmnibox, self.selectedPosition == ToolbarType::kSecondary);
  RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kPositionValidated);
  RecordSelectedPosition(self.selectedPosition,
                         self.selectedPosition == ToolbarType::kPrimary,
                         self.deviceSwitcherResultDispatcher);
}

- (void)discardSelectedPosition {
  RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kPositionDiscarded);
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
  RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kTopOptionSelected);
}

- (void)selectBottomOmnibox {
  self.selectedPosition = ToolbarType::kSecondary;
  RecordScreenEvent(OmniboxPositionChoiceScreenEvent::kBottomOptionSelected);
}

@end
