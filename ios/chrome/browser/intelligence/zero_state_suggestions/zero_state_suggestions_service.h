// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_ZERO_STATE_SUGGESTIONS_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_ZERO_STATE_SUGGESTIONS_SERVICE_H_

#import <UIKit/UIKit.h>

#import <optional>
#import <string>
#import <vector>

#import "base/functional/callback.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/optimization_guide/mojom/model_led_suggestions_service.mojom.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "url/gurl.h"

class GeminiSuggestionHandlerTest;

// Object representing a Gemini suggestion.
@interface ZeroStateSuggestion : NSObject

// Display text for the suggestion.
@property(nonatomic, copy) NSString* text;

// Query to be used when the suggestion is used.
@property(nonatomic, copy) NSString* query;

// Identifier for the icon to be used for the suggestion.
@property(nonatomic, copy) NSString* iconIdentifier;

@end

namespace web {
class WebState;
}  // namespace web

namespace ai {

class ModelLedSuggestionsServiceImpl;

// Service that manages zero-state suggestions, acting as an intermediary
// between GeminiTabHelper and ModelLedSuggestionsServiceImpl.
class ZeroStateSuggestionsService {
 public:
  explicit ZeroStateSuggestionsService(web::WebState* web_state);
  ~ZeroStateSuggestionsService();

  ZeroStateSuggestionsService(const ZeroStateSuggestionsService&) = delete;
  ZeroStateSuggestionsService& operator=(const ZeroStateSuggestionsService&) =
      delete;

  // Fetches zero-state suggestions.
  void FetchZeroStateSuggestions(
      base::OnceCallback<void(NSArray<ZeroStateSuggestion*>*)> callback);

  // Clears cached suggestions.
  void ClearCachedSuggestions();

 private:
  // Adding test classes as friend to facilitate setting state.
  friend class ::GeminiSuggestionHandlerTest;
  friend class ZeroStateSuggestionsServiceTest;

  // Parses the response of a zero-state suggestions execution.
  void ParseSuggestionsResponse(
      base::OnceCallback<void(NSArray<ZeroStateSuggestion*>*)> callback,
      GURL request_url,
      ai::mojom::ModelLedSuggestionsResponseResultPtr result);

  // Builds a suggestions array from raw suggestions.
  NSArray<ZeroStateSuggestion*>* BuildSuggestions(
      const std::vector<std::string>& model_led_suggestions);

  // Helper methods to create static suggestions.
  ZeroStateSuggestion* CreateSummarizeAction();
  ZeroStateSuggestion* CreateFAQAction();
  ZeroStateSuggestion* CreateWhatCanGeminiDoAction();
  ZeroStateSuggestion* CreateCustomAction(NSString* query);

  // Returns whether the "What can Gemini do" action can be shown.
  bool CanShowWhatCanGeminiDoAction();

  // Weak WebState.
  base::WeakPtr<web::WebState> web_state_;

  // The zero-state suggestions service remote.
  mojo::Remote<ai::mojom::ModelLedSuggestionsService> service_;

  // The implementation of the service.
  std::unique_ptr<ai::ModelLedSuggestionsServiceImpl> service_impl_;

  // Cached suggestions for the current page.
  std::optional<std::vector<std::string>> suggestions_;

  // The URL for which the suggestions are cached.
  GURL suggestions_url_;

  // Weak pointer factory.
  base::WeakPtrFactory<ZeroStateSuggestionsService> weak_ptr_factory_{this};
};

}  // namespace ai

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ZERO_STATE_SUGGESTIONS_ZERO_STATE_SUGGESTIONS_SERVICE_H_
