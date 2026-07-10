// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/model/model_led_suggestions_service_impl.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation ZeroStateSuggestion
@end

namespace ai {

namespace {

// The maximum number of actions/suggestions to show.
constexpr NSUInteger kMaxSuggestions = 3;

}  // namespace

ZeroStateSuggestionsService::ZeroStateSuggestionsService(
    web::WebState* web_state) {
  web_state_ = web_state->GetWeakPtr();

  // IsZeroStateSuggestionsEnabled controls model-led suggestions
  if (!IsZeroStateSuggestionsEnabled()) {
    return;
  }

  mojo::PendingReceiver<ai::mojom::ModelLedSuggestionsService> receiver =
      service_.BindNewPipeAndPassReceiver();
  service_impl_ = std::make_unique<ai::ModelLedSuggestionsServiceImpl>(
      std::move(receiver), web_state);
}

ZeroStateSuggestionsService::~ZeroStateSuggestionsService() = default;

void ZeroStateSuggestionsService::FetchZeroStateSuggestions(
    base::OnceCallback<void(NSArray<ZeroStateSuggestion*>*)> callback) {
  if (!web_state_) {
    std::move(callback).Run(BuildSuggestions({}));
    return;
  }

  const GURL request_url = web_state_->GetVisibleURL();

  if (suggestions_.has_value()) {
    // Ensure the cached suggestions are for the current URL.
    if (suggestions_url_ == request_url.GetWithoutRef()) {
      std::move(callback).Run(BuildSuggestions(suggestions_.value()));
    } else {
      // The cached suggestions are stale and thus obsolete.
      std::move(callback).Run(BuildSuggestions({}));
    }
    return;
  }

  if (!service_) {
    std::move(callback).Run(BuildSuggestions({}));
    return;
  }

  base::OnceCallback<void(ai::mojom::ModelLedSuggestionsResponseResultPtr)>
      service_callback = base::BindOnce(
          &ZeroStateSuggestionsService::ParseSuggestionsResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), request_url);

  service_->FetchModelLedSuggestions(std::move(service_callback));
}

void ZeroStateSuggestionsService::ClearCachedSuggestions() {
  suggestions_.reset();
  suggestions_url_ = GURL();
}

void ZeroStateSuggestionsService::ParseSuggestionsResponse(
    base::OnceCallback<void(NSArray<ZeroStateSuggestion*>*)> callback,
    GURL request_url,
    ai::mojom::ModelLedSuggestionsResponseResultPtr result) {
  if (!result || result->is_error()) {
    std::move(callback).Run(nil);
    return;
  }

  std::optional<optimization_guide::proto::ZeroStateSuggestionsResponse>
      response_proto_optional =
          result->get_response()
              .As<optimization_guide::proto::ZeroStateSuggestionsResponse>();
  if (!response_proto_optional.has_value()) {
    std::move(callback).Run(nil);
    return;
  }
  optimization_guide::proto::ZeroStateSuggestionsResponse response_proto =
      response_proto_optional.value();

  suggestions_.emplace();
  for (const auto& suggestion : response_proto.suggestions()) {
    suggestions_->push_back(suggestion.label());
  }
  suggestions_url_ = request_url.GetWithoutRef();

  std::move(callback).Run(BuildSuggestions(suggestions_.value()));
}

NSArray<ZeroStateSuggestion*>* ZeroStateSuggestionsService::BuildSuggestions(
    const std::vector<std::string>& model_led_suggestions) {
  NSMutableArray<ZeroStateSuggestion*>* actions = [NSMutableArray array];
  if (IsZeroStateSuggestionsCentralizationEnabled()) {
    // 1. Always "Summarize" static chip.
    [actions addObject:CreateSummarizeAction()];

    // 2. If model led chip is not available, fall back to "Create FAQ"
    // static chip.
    if (model_led_suggestions.empty()) {
      [actions addObject:CreateFAQAction()];
    }

    // 3. Show WCGD if available.
    if (CanShowWhatCanGeminiDoAction()) {
      [actions addObject:CreateWhatCanGeminiDoAction()];
    }
  }

  // Append model-provided suggestions up to the max actions
  for (const std::string& suggestion : model_led_suggestions) {
    if (actions.count >= kMaxSuggestions) {
      break;
    }
    [actions addObject:CreateCustomAction(base::SysUTF8ToNSString(suggestion))];
  }
  return actions;
}

ZeroStateSuggestion* ZeroStateSuggestionsService::CreateSummarizeAction() {
  ZeroStateSuggestion* suggestion = [[ZeroStateSuggestion alloc] init];
  suggestion.text =
      l10n_util::GetNSString(IDS_IOS_ZERO_STATE_SUGGESTIONS_SUMMARIZE_TEXT);
  suggestion.query =
      l10n_util::GetNSString(IDS_IOS_ZERO_STATE_SUGGESTIONS_SUMMARIZE_QUERY);
  suggestion.iconIdentifier = nil;
  return suggestion;
}

ZeroStateSuggestion* ZeroStateSuggestionsService::CreateFAQAction() {
  ZeroStateSuggestion* suggestion = [[ZeroStateSuggestion alloc] init];
  suggestion.text =
      l10n_util::GetNSString(IDS_IOS_ZERO_STATE_SUGGESTIONS_FAQ_TEXT);
  suggestion.query =
      l10n_util::GetNSString(IDS_IOS_ZERO_STATE_SUGGESTIONS_FAQ_QUERY);
  suggestion.iconIdentifier = nil;
  return suggestion;
}

ZeroStateSuggestion*
ZeroStateSuggestionsService::CreateWhatCanGeminiDoAction() {
  ZeroStateSuggestion* suggestion = [[ZeroStateSuggestion alloc] init];
  suggestion.text = l10n_util::GetNSString(
      IDS_IOS_ZERO_STATE_SUGGESTIONS_WHAT_CAN_GEMINI_DO_TEXT);
  suggestion.query = l10n_util::GetNSString(
      IDS_IOS_ZERO_STATE_SUGGESTIONS_WHAT_CAN_GEMINI_DO_QUERY);
  suggestion.iconIdentifier = nil;
  return suggestion;
}

ZeroStateSuggestion* ZeroStateSuggestionsService::CreateCustomAction(
    NSString* query) {
  ZeroStateSuggestion* suggestion = [[ZeroStateSuggestion alloc] init];
  suggestion.text = query;
  suggestion.query = query;
  suggestion.iconIdentifier = nil;
  return suggestion;
}

bool ZeroStateSuggestionsService::CanShowWhatCanGeminiDoAction() {
  if (!web_state_ || !IsZeroStateSuggestionsWCGDEnabled()) {
    return false;
  }
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  return tracker && tracker->WouldTriggerHelpUI(
                        feature_engagement::kIPHiOSGeminiWhatCanGeminiDo);
}

}  // namespace ai
