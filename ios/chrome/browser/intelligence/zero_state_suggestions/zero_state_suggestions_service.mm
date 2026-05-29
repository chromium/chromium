// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/model/model_led_suggestions_service_impl.h"
#import "ios/web/public/web_state.h"

namespace {

NSMutableArray<NSString*>* ZeroStateSuggestionsAsNSArray(
    const std::vector<std::string>& suggestions) {
  NSMutableArray<NSString*>* ns_suggestions =
      [NSMutableArray arrayWithCapacity:suggestions.size()];
  for (const std::string& suggestion : suggestions) {
    [ns_suggestions addObject:base::SysUTF8ToNSString(suggestion)];
  }
  return ns_suggestions;
}

}  // namespace

namespace ai {

ZeroStateSuggestionsService::ZeroStateSuggestionsService(
    web::WebState* web_state) {
  web_state_ = web_state->GetWeakPtr();
  mojo::PendingReceiver<ai::mojom::ModelLedSuggestionsService> receiver =
      service_.BindNewPipeAndPassReceiver();
  service_impl_ = std::make_unique<ai::ModelLedSuggestionsServiceImpl>(
      std::move(receiver), web_state);
}

ZeroStateSuggestionsService::~ZeroStateSuggestionsService() = default;

void ZeroStateSuggestionsService::FetchZeroStateSuggestions(
    base::OnceCallback<void(NSArray<NSString*>*)> callback) {
  if (!can_apply_ || !web_state_) {
    std::move(callback).Run(nil);
    return;
  }

  const GURL request_url = web_state_->GetVisibleURL();

  if (suggestions_.has_value()) {
    // Ensure the cached suggestions are for the current URL.
    if (suggestions_url_ == request_url.GetWithoutRef()) {
      std::move(callback).Run(
          ZeroStateSuggestionsAsNSArray(suggestions_.value()));
    } else {
      // The cached suggestions are stale and thus obsolete.
      std::move(callback).Run(nil);
    }
    return;
  }

  if (!service_) {
    std::move(callback).Run(nil);
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
  can_apply_ = false;
}

void ZeroStateSuggestionsService::SetCanApply(bool can_apply) {
  can_apply_ = can_apply;
}

bool ZeroStateSuggestionsService::CanApply() const {
  return can_apply_;
}

void ZeroStateSuggestionsService::ParseSuggestionsResponse(
    base::OnceCallback<void(NSArray<NSString*>*)> callback,
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

  std::move(callback).Run(ZeroStateSuggestionsAsNSArray(suggestions_.value()));
}

}  // namespace ai
