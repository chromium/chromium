// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/row/actions/omnibox_popup_actions_row_content_configuration.h"

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/omnibox_popup_actions_row_content_view.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/omnibox_popup_actions_row_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/suggest_action.h"
#import "net/base/apple/url_conversions.h"

@interface OmniboxPopupActionsRowContentConfiguration ()

// Redefined as readwrite.
@property(nonatomic, strong, readwrite) NSArray<SuggestAction*>* actions;

@end

@implementation OmniboxPopupActionsRowContentConfiguration

/// Layout this cell with the given data before displaying.
+ (instancetype)cellConfiguration {
  return [[OmniboxPopupActionsRowContentConfiguration alloc] init];
}

- (void)setSuggestion:(id<AutocompleteSuggestion>)suggestion {
  [super setSuggestion:suggestion];

  _actions = suggestion.actionsInSuggest;
}

#pragma mark - UIContentConfiguration

- (id)copyWithZone:(NSZone*)zone {
  __typeof__(self) configuration = [super copyWithZone:zone];
  configuration.actions = self.actions;
  return configuration;
}

- (UIView<UIContentView>*)makeContentView {
  return [[OmniboxPopupActionsRowContentView alloc] initWithConfiguration:self];
}

/// Updates the configuration for different state of the view. This contains
/// everything that can change in the configuration's lifetime.
- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  OmniboxPopupActionsRowContentConfiguration* configuration =
      [super updatedConfigurationForState:state];
  configuration.actions = self.actions;
  return configuration;
}

@end
