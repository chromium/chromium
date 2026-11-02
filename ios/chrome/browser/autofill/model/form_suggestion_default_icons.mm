// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/model/form_suggestion_default_icons.h"

#import <UIKit/UIKit.h>

#import "base/feature_list.h"
#import "base/types/optional_ref.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/web_state.h"

namespace {

// Point size of the SF Symbol used for default icons.
constexpr CGFloat kSymbolPointSize = 17.0;

// Returns the default icon for the suggestion type.
UIImage* DefaultIconForType(FormSuggestion* suggestion,
                            web::WebState* web_state) {
  switch (suggestion.type) {
    case autofill::SuggestionType::kUndo:
      return SymbolWithPalette(
          SymbolWithPointSize(SymbolArrowUTurnBackward, kSymbolActionPointSize),
          @[
            [UIColor colorNamed:kTextPrimaryColor],
          ]);
    case autofill::SuggestionType::kGeneratePasswordEntry:
      return MakeSymbolMulticolor(
          SymbolWithPointSize(SymbolPasswordManager, kSymbolPointSize));
    case autofill::SuggestionType::kAddressEntry: {
      switch (suggestion.suggestionIconType) {
        case SuggestionIconType::kAccountHome:
          return SymbolWithPalette(
              SymbolWithPointSize(SymbolHome, kSymbolPointSize), @[
                [UIColor colorNamed:kTextPrimaryColor],
              ]);
        case SuggestionIconType::kAccountWork:
          return SymbolWithPalette(
              SymbolWithPointSize(SymbolWork, kSymbolPointSize), @[
                [UIColor colorNamed:kTextPrimaryColor],
              ]);
        default:
          return nil;
      }
    }
    case autofill::SuggestionType::kFillAutofillAi: {
      if (!web_state) {
        return nil;
      }

      if (base::FeatureList::IsEnabled(
              autofill::features::kAutofillAiNoFillingIconsExperiment)) {
        return nil;
      }

      base::optional_ref<const autofill::EntityInstance> entity =
          autofill::GetEntityInstance(
              ProfileIOS::FromBrowserState(web_state->GetBrowserState()),
              suggestion.payload);
      if (!entity.has_value()) {
        return nil;
      }

      const bool isPersonalContext =
          entity->record_type() ==
          autofill::EntityInstance::RecordType::kPersonalContext;

      return autofill::DefaultIconForAutofillAiEntityType(
          entity->type().name(), isPersonalContext, kSymbolPointSize,
          /*tint_color=*/nil);
    }
    case autofill::SuggestionType::kAutocompleteAtMemoryButton:
      return SymbolWithPalette(SymbolWithPointSize(SymbolMagnifyingglassSpark,
                                                   kSymbolActionPointSize),
                               @[ [UIColor colorNamed:kTextPrimaryColor] ]);
    case autofill::SuggestionType::kAutocompleteEntry:
    default:
      return nil;
  }
}

}  // namespace

NSArray<FormSuggestion*>* CopyFormSuggestionsWithDefaultIcons(
    NSArray<FormSuggestion*>* suggestions,
    web::WebState* web_state) {
  NSMutableArray<FormSuggestion*>* suggestionsCopy = [NSMutableArray array];
  for (FormSuggestion* suggestion : suggestions) {
    UIImage* defaultIcon = DefaultIconForType(suggestion, web_state);

    // If the suggestion lacks an icon and a default icon exists for its type,
    // create a copy with the default icon; otherwise, preserve the existing
    // suggestion as-is.
    BOOL shouldUpdateIcon = !suggestion.icon && defaultIcon;

    if (shouldUpdateIcon) {
      FormSuggestion* suggestionCopy = [FormSuggestion
                  suggestionWithValue:suggestion.value
                           minorValue:suggestion.minorValue
                   displayDescription:suggestion.displayDescription
                                 icon:defaultIcon
                                 type:suggestion.type
                              payload:suggestion.payload
          fieldByFieldFillingTypeUsed:suggestion.fieldByFieldFillingTypeUsed
                       requiresReauth:suggestion.requiresReauth
           acceptanceA11yAnnouncement:suggestion.acceptanceA11yAnnouncement
                             metadata:suggestion.metadata];
      // TODO(crbug.com/452315148): Include `featureForIPH` in the
      // `FormSuggestion` constructor.
      suggestionCopy.featureForIPH = suggestion.featureForIPH;
      suggestionCopy.suggestionIconType = suggestion.suggestionIconType;
      suggestionCopy = [FormSuggestion copy:suggestionCopy
                               andSetParams:suggestion.params
                                   provider:suggestion.provider];
      [suggestionsCopy addObject:suggestionCopy];
    } else {
      [suggestionsCopy addObject:suggestion];
    }
  }
  return suggestionsCopy;
}
