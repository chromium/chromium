// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_granular_fill_mediator.h"

#import <optional>

#import "base/check.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_fill_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"
#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"
#import "ios/chrome/browser/autofill/public/autofill_settings_navigator.h"

using autofill::Suggestion;

@implementation AtMemoryGranularFillMediator {
  // Suggestion containing attributes to display.
  std::optional<Suggestion> _suggestion;
  // Settings page opened by the "Manage enhanced autofill" row. Defaults to
  // the Enhanced Autofill page when `_suggestion` has no more specific
  // destination, as the row is always displayed.
  AutofillSettingsPage _settingsPage;
}

- (instancetype)initWithSuggestion:(Suggestion&&)suggestion {
  self = [super init];
  if (self) {
    _suggestion = std::move(suggestion);
    _settingsPage = AutofillSettingsPageForAtMemorySuggestion(*_suggestion)
                        .value_or(AutofillSettingsPage::kEnhancedAutofill);
  }
  return self;
}

- (void)setConsumer:(id<AtMemoryGranularFillConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (!_consumer) {
    return;
  }

  CHECK(_suggestion);
  [_consumer setTitle:GetAtMemoryGranularFillTitle(*_suggestion)];
  [_consumer setGranularFillItems:AtMemoryGranularFillItemsForSuggestion(
                                      *_suggestion)];
}

#pragma mark - AtMemoryGranularFillMutator

- (void)didSelectGranularFillItem:(AtMemoryGranularFillItem*)item {
  if (!item || item.index < 0 || !_suggestion ||
      static_cast<size_t>(item.index) >= _suggestion->children.size()) {
    [self.atMemoryHandler dismissAtMemory];
    return;
  }

  [self.fillHandler fillWithSuggestion:_suggestion->children[item.index]];
}

- (void)didSelectManageEnhancedAutofillItem {
  [self.settingsNavigator openSettingsForPage:_settingsPage];
}

@end
