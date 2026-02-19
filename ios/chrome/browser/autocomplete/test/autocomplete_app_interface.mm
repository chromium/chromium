// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/test/autocomplete_app_interface.h"

#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/autocomplete/test/fake_suggestions_autocomplete_controller.h"
#import "ios/chrome/browser/autocomplete/test/fake_suggestions_builder.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation AutocompleteAppInterface

+ (void)enableFakeSuggestionsInContext:(OmniboxPresentationContext)context {
  Browser* browser = chrome_test_util::GetMainBrowser();
  // Ensure the service is created.
  AutocompleteBrowserAgent::CreateForBrowser(browser);

  // Setup the fake suggestions autocomplete controller.
  auto autocompleteController =
      std::make_unique<FakeSuggestionsAutocompleteController>();
  AutocompleteBrowserAgent::FromBrowser(browser)
      ->SetAutocompleteControllerForTesting(context,
                                            std::move(autocompleteController));
}

#pragma mark - Adding suggestions

+ (void)addURLShortcutMatch:(NSString*)shortcutText
       destinationURLString:(NSString*)URLString
                    context:(OmniboxPresentationContext)context {
  GURL shortcutURL = GURL(URLString.cr_UTF16String);
  // Shortcut URL must be valid.
  CHECK(shortcutURL.is_valid());
  FakeSuggestionsAutocompleteController* controller =
      [self fakeControllerFromContext:context];
  FakeSuggestionsBuilder* builder = controller->fake_suggestions_builder();
  builder->AddURLShortcut(shortcutText.cr_UTF16String,
                          URLString.cr_UTF16String);
}

+ (void)addSearchSuggestion:(NSString*)query
                    context:(OmniboxPresentationContext)context {
  FakeSuggestionsAutocompleteController* controller =
      [self fakeControllerFromContext:context];
  FakeSuggestionsBuilder* builder = controller->fake_suggestions_builder();
  builder->AddSearchSuggestion(query.cr_UTF16String);
}

+ (void)addHistoryURLSuggestion:(NSString*)title
           destinationURLString:(NSString*)URLString
                        context:(OmniboxPresentationContext)context {
  GURL destinationURL = GURL(URLString.cr_UTF16String);
  // Destination URL must be valid.
  CHECK(destinationURL.is_valid());
  FakeSuggestionsAutocompleteController* controller =
      [self fakeControllerFromContext:context];
  FakeSuggestionsBuilder* builder = controller->fake_suggestions_builder();
  builder->AddHistoryURLSuggestion(title.cr_UTF16String,
                                   URLString.cr_UTF16String);
}

#pragma mark - Private

+ (FakeSuggestionsAutocompleteController*)fakeControllerFromContext:
    (OmniboxPresentationContext)context {
  Browser* browser = chrome_test_util::GetMainBrowser();
  AutocompleteBrowserAgent* autocomplete_agent =
      AutocompleteBrowserAgent::FromBrowser(browser);
  AutocompleteController* controller =
      autocomplete_agent->GetAutocompleteController(context);
  // This assumes the controller is indeed a
  // FakeSuggestionsAutocompleteController. Tests using this should ensure that
  // `enableFakeSuggestionsInContext` was called.
  return static_cast<FakeSuggestionsAutocompleteController*>(controller);
}

@end
