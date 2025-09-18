// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROTOTYPE_GEMINI_PROTOTYPE_OMNIBOX_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROTOTYPE_GEMINI_PROTOTYPE_OMNIBOX_SERVICE_IOS_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/omnibox/browser/gemini_prototype_omnibox_service.h"
#include "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#include "ios/chrome/browser/optimization_guide/mojom/ai_prototyping_service.mojom.h"
#include "ios/chrome/browser/shared/model/browser/browser_list.h"
#include "ios/chrome/browser/shared/model/browser/browser_list_observer.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

class Browser;
class ProfileIOS;
class WebStateList;

namespace web {
class WebState;
class NavigationContext;
}  // namespace web

namespace ai {
class AIPrototypingServiceImpl;
}  // namespace ai

// iOS implementation of GeminiPrototypeOmniboxService.
// Note: This service requires the user to be signed in with a Google corp
// account to query the Gemini backend.
class GeminiPrototypeOmniboxServiceIOS : public GeminiPrototypeOmniboxService,
                                         public BrowserListObserver,
                                         public WebStateListObserver,
                                         public web::WebStateObserver {
 public:
  explicit GeminiPrototypeOmniboxServiceIOS(ProfileIOS* profile);
  ~GeminiPrototypeOmniboxServiceIOS() override;

  // GeminiPrototypeOmniboxService:
  void RequestSuggestions(const AutocompleteInput& input,
                          SuggestionCallback callback) override;

 private:
  // BrowserListObserver:
  void OnBrowserAdded(const BrowserList* browser_list,
                      Browser* browser) override;
  void OnBrowserRemoved(const BrowserList* browser_list,
                        Browser* browser) override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // web::WebStateObserver:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Starts a prefetch request for the given `web_state`.
  void PrefetchSuggestion(web::WebState* web_state);

  // Callback for when the prefetched page context has been retrieved.
  void OnPrefetchPageContextRetrieved(
      const GURL& url,
      PageContextWrapperCallbackResponse response);

  // Callback for when the prefetched suggestion has been received.
  void OnPrefetchSuggestionReceived(const GURL& url,
                                    const std::string& response_string);

  raw_ptr<ProfileIOS> profile_;
  // The active WebState this service is observing.
  raw_ptr<web::WebState> observed_web_state_ = nullptr;

  // The last URL for which a suggestion was successfully fetched.
  GURL cached_suggestion_url_;
  // The last successfully fetched suggestion.
  std::u16string cached_suggestion_;

  // Mojo related service and service implementations. Kept alive to have an
  // existing implementation instance during the lifecycle of the mediator.
  // Remote used to make calls to functions related to
  // `AIProprototypingService`.
  mojo::Remote<ai::mojom::AIPrototypingService> ai_prototyping_service_;
  // Instantiated to pipe virtual remote calls to overridden functions in the
  // `AIPrototypingServiceImpl`.
  std::unique_ptr<ai::AIPrototypingServiceImpl> ai_prototyping_service_impl_;

  // The PageContext wrapper for prefetch requests.
  PageContextWrapper* page_context_wrapper_;

  // The URL of the current in-flight prefetch request.
  GURL pending_prefetch_url_;

  // The callback for a suggestion that was requested while a prefetch was in
  // progress for `pending_prefetch_url_`.
  SuggestionCallback pending_callback_;

  // Scoped observations.
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
  base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>
      web_state_list_observations_{this};
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};

  base::WeakPtrFactory<GeminiPrototypeOmniboxServiceIOS> weak_ptr_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_MODEL_PROTOTYPE_GEMINI_PROTOTYPE_OMNIBOX_SERVICE_IOS_H_
