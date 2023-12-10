// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_consumer.h"
#import "ios/chrome/browser/ui/first_run/omnibox_position/omnibox_position_choice_util.h"

@interface OmniboxPositionChoiceMediator ()

/// The selected omnibox position.
@property(nonatomic, assign) ToolbarType selectedPosition;

@end

@implementation OmniboxPositionChoiceMediator

- (instancetype)init {
  self = [super init];
  if (self) {
    _selectedPosition = DefaultSelectedOmniboxPosition();
  }
  return self;
}

- (void)saveSelectedPosition {
  if (self.originalPrefService) {
    _originalPrefService->SetBoolean(
        prefs::kBottomOmnibox,
        self.selectedPosition == ToolbarType::kSecondary);
  }
  // TODO(crbug.com/1503638): Record selected position histogram.
}

- (void)discardSelectedPosition {
  // TODO(crbug.com/1503638): Record selected position histogram.
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
  // TODO(crbug.com/1503638): Recoard user action.
}

- (void)selectBottomOmnibox {
  self.selectedPosition = ToolbarType::kSecondary;
  // TODO(crbug.com/1503638): Record user action.
}

@end
