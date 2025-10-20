// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/model/prototype/gemini_prototype_omnibox_service_ios.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/optimization_guide/core/optimization_guide_model_executor.h"
#import "components/optimization_guide/proto/features/bling_prototyping.pb.h"
#import "ios/chrome/browser/ai_prototyping/model/ai_prototyping_service_impl.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/bindings/pending_receiver.h"

namespace {

// Creates a query string with context for the Gemini API.
std::string CreateQuery(const AutocompleteInput& input) {
  return base::UTF16ToUTF8(base::SysNSStringToUTF16([NSString
      stringWithFormat:@"I am on a page %@, give me one suggestion for a "
                       @"query based on the context of the page. Only "
                       @"return the suggested query.",
                       base::SysUTF8ToNSString(input.current_url().spec())]));
}

}  // namespace

#pragma mark - GeminiPrototypeOmniboxServiceIOS

GeminiPrototypeOmniboxServiceIOS::GeminiPrototypeOmniboxServiceIOS(
    ProfileIOS* profile)
    : profile_(profile),
      browser_list_observation_(this),
      web_state_list_observations_(this),
      web_state_observations_(this) {
  DCHECK(!profile_->IsOffTheRecord());
  mojo::PendingReceiver<ai::mojom::AIPrototypingService>
      ai_prototyping_receiver =
          ai_prototyping_service_.BindNewPipeAndPassReceiver();
  ai_prototyping_service_impl_ = std::make_unique<ai::AIPrototypingServiceImpl>(
      std::move(ai_prototyping_receiver), profile,
      /*start_on_device=*/false);

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
  browser_list_observation_.Observe(browser_list);
  for (Browser* browser :
       browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    web_state_list_observations_.AddObservation(browser->GetWebStateList());
    if (browser->GetWebStateList()->GetActiveWebState()) {
      observed_web_state_ = browser->GetWebStateList()->GetActiveWebState();
      web_state_observations_.AddObservation(observed_web_state_);
    }
  }
}

GeminiPrototypeOmniboxServiceIOS::~GeminiPrototypeOmniboxServiceIOS() = default;

#pragma mark - GeminiPrototypeOmniboxService

void GeminiPrototypeOmniboxServiceIOS::RequestSuggestions(
    const AutocompleteInput& input,
    SuggestionCallback callback) {
  CHECK(omnibox::IsGeminiPrototypeProviderEnabled());

  if (input.current_url() == cached_suggestion_url_ &&
      !cached_suggestion_.empty()) {
    std::move(callback).Run(cached_suggestion_);
    return;
  }

  if (input.current_url() != cached_suggestion_url_) {
    cached_suggestion_ = u"";
    cached_suggestion_url_ = GURL();
  }

  if (input.current_url() == pending_prefetch_url_ &&
      !pending_prefetch_url_.is_empty()) {
    // A prefetch is in progress for this URL. Store the callback to be
    // executed when it finishes.
    pending_callback_ = std::move(callback);
    return;
  }

  // No cached suggestion and no prefetch in progress.
  std::move(callback).Run(u"");
}

#pragma mark - BrowserListObserver

void GeminiPrototypeOmniboxServiceIOS::OnBrowserAdded(
    const BrowserList* browser_list,
    Browser* browser) {
  if (browser->GetProfile() != profile_ ||
      browser->type() != Browser::Type::kRegular) {
    return;
  }
  web_state_list_observations_.AddObservation(browser->GetWebStateList());
}

void GeminiPrototypeOmniboxServiceIOS::OnBrowserRemoved(
    const BrowserList* browser_list,
    Browser* browser) {
  if (browser->GetProfile() != profile_ ||
      browser->type() != Browser::Type::kRegular) {
    return;
  }
  web_state_list_observations_.RemoveObservation(browser->GetWebStateList());
}

#pragma mark - WebStateListObserver

void GeminiPrototypeOmniboxServiceIOS::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (!status.active_web_state_change()) {
    return;
  }

  if (observed_web_state_) {
    web_state_observations_.RemoveObservation(observed_web_state_);
  }
  observed_web_state_ = web_state_list->GetActiveWebState();
  if (observed_web_state_) {
    web_state_observations_.AddObservation(observed_web_state_);
    PrefetchSuggestion(observed_web_state_);
  }
}

#pragma mark - web::WebStateObserver

void GeminiPrototypeOmniboxServiceIOS::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->IsSameDocument() ||
      !navigation_context->HasCommitted() || navigation_context->GetError()) {
    return;
  }
  PrefetchSuggestion(web_state);
}

void GeminiPrototypeOmniboxServiceIOS::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(observed_web_state_, web_state);
  web_state_observations_.RemoveObservation(observed_web_state_);
  observed_web_state_ = nullptr;
}

#pragma mark - Private

void GeminiPrototypeOmniboxServiceIOS::PrefetchSuggestion(
    web::WebState* web_state) {
  if (!web_state) {
    return;
  }
  const GURL& url = web_state->GetVisibleURL();

  if (url == pending_prefetch_url_) {
    return;
  }

  // Invalidate any pending callbacks from a previous prefetch and clear state.
  weak_ptr_factory_.InvalidateWeakPtrs();
  pending_prefetch_url_ = url;
  pending_callback_.Reset();
  cached_suggestion_ = u"";
  cached_suggestion_url_ = GURL();

  base::OnceCallback<void(PageContextWrapperCallbackResponse)>
      page_context_completion_callback = base::BindOnce(
          &GeminiPrototypeOmniboxServiceIOS::OnPrefetchPageContextRetrieved,
          weak_ptr_factory_.GetWeakPtr(), url);

  page_context_wrapper_ = [[PageContextWrapper alloc]
        initWithWebState:web_state
      completionCallback:std::move(page_context_completion_callback)];
  [page_context_wrapper_ setShouldGetAnnotatedPageContent:YES];
  [page_context_wrapper_ setShouldGetSnapshot:YES];
  [page_context_wrapper_ populatePageContextFieldsAsync];
}

void GeminiPrototypeOmniboxServiceIOS::OnPrefetchPageContextRetrieved(
    const GURL& url,
    PageContextWrapperCallbackResponse response) {
  page_context_wrapper_ = nil;

  // If the page context could not be retrieved, notify any pending callbacks
  // and clean up.
  if (!response.has_value() && !url.SchemeIsHTTPOrHTTPS()) {
    pending_prefetch_url_ = GURL();
    if (pending_callback_) {
      std::move(pending_callback_).Run(u"");
    }
    return;
  }

  optimization_guide::proto::BlingPrototypingRequest request;
  AutocompleteInput input;
  input.set_current_url(url);
  request.set_query(CreateQuery(input));
  if (response.has_value()) {
    request.set_allocated_page_context(response.value().release());
  } else if (url.SchemeIsHTTPOrHTTPS()) {
    request.mutable_page_context()->set_url(url.spec());
  }
  request.set_temperature(2.0f);
  request.set_model_enum(
      static_cast<optimization_guide::proto::BlingPrototypingRequest_ModelEnum>(
          5));  // MODEL_ENUM_EVERGREEN_GEMINI_V3

  ai_prototyping_service_->ExecuteServerQuery(
      mojo_base::ProtoWrapper(request),
      base::BindOnce(
          &GeminiPrototypeOmniboxServiceIOS::OnPrefetchSuggestionReceived,
          weak_ptr_factory_.GetWeakPtr(), url));
}

void GeminiPrototypeOmniboxServiceIOS::OnPrefetchSuggestionReceived(
    const GURL& url,
    const std::string& response_string) {
  std::u16string suggestion = base::UTF8ToUTF16(response_string);
  cached_suggestion_url_ = url;
  cached_suggestion_ = suggestion;

  if (url == pending_prefetch_url_ && pending_callback_) {
    std::move(pending_callback_).Run(suggestion);
  }

  // Clear the pending state, as the request is now complete.
  pending_prefetch_url_ = GURL();
  pending_callback_.Reset();
}
