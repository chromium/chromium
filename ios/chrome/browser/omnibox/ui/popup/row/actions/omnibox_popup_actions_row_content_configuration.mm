// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/row/actions/omnibox_popup_actions_row_content_configuration.h"

#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/model/suggestions/suggest_action.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/actions/omnibox_popup_actions_row_content_view.h"
#import "ios/chrome/browser/omnibox/ui/popup/row/actions/omnibox_popup_actions_row_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "net/base/apple/url_conversions.h"

// Redefined as readwrite.
@interface OmniboxPopupActionsRowContentConfiguration ()

@property(nonatomic, strong, readwrite) NSArray<SuggestAction*>* actions;
@property(nonatomic, readwrite) NSUInteger highlightedActionIndex;
@property(nonatomic, assign, readwrite) BOOL isBackgroundHighlighted;
@property(nonatomic, assign, readwrite) BOOL leadingIconHighlighted;
@property(nonatomic, strong, readwrite) NSAttributedString* primaryText;
@property(nonatomic, strong, readwrite) NSAttributedString* secondaryText;

@end

@implementation OmniboxPopupActionsRowContentConfiguration

@synthesize isBackgroundHighlighted;
@synthesize leadingIconHighlighted;
@synthesize primaryText;
@synthesize secondaryText;

/// Layout this cell with the given data before displaying.
+ (instancetype)cellConfiguration {
  return [[OmniboxPopupActionsRowContentConfiguration alloc] init];
}

- (void)setSuggestion:(id<AutocompleteSuggestion>)suggestion {
  [super setSuggestion:suggestion];

  _actions = suggestion.actionsInSuggest;
  _highlightedActionIndex = NSNotFound;
}

- (void)highlightNextActionButton {
  if (_highlightedActionIndex == _actions.count - 1) {
    return;
  }
  _highlightedActionIndex = _highlightedActionIndex + 1;
}

- (void)highlightPreviousActionButton {
  if (_highlightedActionIndex == 0) {
    return;
  }
  _highlightedActionIndex = _highlightedActionIndex - 1;
}

#pragma mark OmniboxKeyboardDelegate

- (BOOL)canPerformKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  using enum OmniboxKeyboardAction;
  switch (keyboardAction) {
    case kUpArrow: {
      if (self.isBackgroundHighlighted) {
        return NO;
      }
      return YES;
    }
    case kDownArrow: {
      if (self.highlightedActionIndex != NSNotFound) {
        return NO;
      }
      return YES;
    }
    case kLeftArrow:
    case kRightArrow:
      return YES;
    case kReturnKey:
      return [self canPerformReturnKeyAction];
  }
}

- (void)performKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  using enum OmniboxKeyboardAction;
  switch (keyboardAction) {
    case kUpArrow: {
      self.highlightedActionIndex = NSNotFound;
      break;
    }
    case kDownArrow: {
      self.highlightedActionIndex = 0;
      break;
    }
    case kLeftArrow:
    case kRightArrow: {
      if (self.isBackgroundHighlighted) {
        self.highlightedActionIndex = 0;
      } else {
        BOOL isRTL =
            [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                        self.semanticContentAttribute] ==
            UIUserInterfaceLayoutDirectionRightToLeft;

        OmniboxKeyboardAction nextActionButton =
            isRTL ? kLeftArrow : kRightArrow;
        OmniboxKeyboardAction previousActionButton =
            isRTL ? kRightArrow : kLeftArrow;

        if (keyboardAction == nextActionButton) {
          if (self.highlightedActionIndex == self.actions.count - 1) {
            break;
          }
          self.highlightedActionIndex = self.highlightedActionIndex + 1;
        } else if (keyboardAction == previousActionButton) {
          if (self.highlightedActionIndex == 0) {
            break;
          }
          self.highlightedActionIndex = self.highlightedActionIndex - 1;
        }
      }
      break;
    }
    case kReturnKey:
      [self performReturnKeyAction];
      break;
  }
}

#pragma mark - UIContentConfiguration

- (id)copyWithZone:(NSZone*)zone {
  __typeof__(self) configuration = [super copyWithZone:zone];
  configuration.actions = self.actions;
  configuration.highlightedActionIndex = self.highlightedActionIndex;
  return configuration;
}

- (UIView<UIContentView>*)makeContentView {
  return [[OmniboxPopupActionsRowContentView alloc] initWithConfiguration:self];
}

/// Updates the configuration for different state of the view. This contains
/// everything that can change in the configuration's lifetime.
- (instancetype)updatedConfigurationForState:
    (UIViewConfigurationState*)viewState {
  OmniboxPopupActionsRowContentConfiguration* configuration =
      [super updatedConfigurationForState:viewState];

  const BOOL isHighlighted = (viewState.highlighted || viewState.selected) &&
                             (self.highlightedActionIndex == NSNotFound);

  configuration.isBackgroundHighlighted = isHighlighted;
  configuration.leadingIconHighlighted = isHighlighted;
  configuration.primaryText =
      isHighlighted
          ? [self.class highlightedAttributedStringWithString:self.suggestion
                                                                  .text]
          : self.suggestion.text;
  configuration.secondaryText =
      isHighlighted
          ? [self.class highlightedAttributedStringWithString:self.suggestion
                                                                  .detailText]
          : self.suggestion.detailText;
  configuration.actions = self.actions;

  if (!viewState.highlighted && !viewState.selected) {
    configuration.highlightedActionIndex = NSNotFound;
  } else {
    configuration.highlightedActionIndex = self.highlightedActionIndex;
  }

  return configuration;
}

#pragma mark - Private

/// Whether the Return/Enter action can be performed.
- (BOOL)canPerformReturnKeyAction {
  return self.highlightedActionIndex != NSNotFound;
}

/// Performs Return/Enter action.
- (void)performReturnKeyAction {
  CHECK([self canPerformReturnKeyAction]);
  CHECK(self.highlightedActionIndex < self.actions.count);

  SuggestAction* action = self.actions[self.highlightedActionIndex];
  [self.delegate omniboxPopupRowActionSelectedWithConfiguration:self
                                                         action:action];
}

@end
