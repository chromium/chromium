// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram.h"
#import "base/strings/utf_string_conversions.h"
#import "base/trace_event/trace_event.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller_emitter.h"
#import "components/omnibox/browser/autocomplete_enums.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_type.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/omnibox/browser/omnibox_popup_view.h"
#import "components/omnibox/browser/page_classification_functions.h"
#import "components/omnibox/common/omnibox_feature_configs.h"
#import "components/search_engines/template_url_starter_pack_data.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_view_ios.h"
#import "ui/gfx/geometry/rect.h"

OmniboxControllerIOS::OmniboxControllerIOS(
    OmniboxViewIOS* view,
    std::unique_ptr<OmniboxClient> client,
    base::TimeDelta autocomplete_stop_timer_duration)
    : client_(std::move(client)),
      autocomplete_controller_(std::make_unique<AutocompleteController>(
          client_->CreateAutocompleteProviderClient(),
          AutocompleteClassifier::DefaultOmniboxProviders(),
          autocomplete_stop_timer_duration)),
      edit_model_(std::make_unique<OmniboxEditModelIOS>(
          /*omnibox_controller=*/this,
          view)) {

  // Register the `AutocompleteController` with `AutocompleteControllerEmitter`.
  if (auto* emitter = client_->GetAutocompleteControllerEmitter()) {
    autocomplete_controller_->AddObserver(emitter);
  }
}

constexpr bool is_ios = !!BUILDFLAG(IS_IOS);

OmniboxControllerIOS::~OmniboxControllerIOS() = default;

void OmniboxControllerIOS::StartAutocomplete(
    const AutocompleteInput& input) const {
  TRACE_EVENT0("omnibox", "OmniboxControllerIOS::StartAutocomplete");

  // We don't explicitly clear OmniboxPopupModel::manually_selected_match, as
  // Start ends up invoking OmniboxPopupModel::OnResultChanged which clears it.
  autocomplete_controller_->Start(input);
}

void OmniboxControllerIOS::StopAutocomplete(bool clear_result) const {
  TRACE_EVENT0("omnibox", "OmniboxControllerIOS::StopAutocomplete");
  autocomplete_controller_->Stop(clear_result
                                     ? AutocompleteStopReason::kClobbered
                                     : AutocompleteStopReason::kInteraction);
}

void OmniboxControllerIOS::StartZeroSuggestPrefetch() {
  TRACE_EVENT0("omnibox", "OmniboxControllerIOS::StartZeroSuggestPrefetch");
  auto page_classification =
      client_->GetPageClassification(/*is_prefetch=*/true);

  GURL current_url = client_->GetURL();
  std::u16string text = base::UTF8ToUTF16(current_url.spec());

  if (omnibox::IsNTPPage(page_classification) || !is_ios) {
    text.clear();
  }

  AutocompleteInput input(text, page_classification,
                          client_->GetSchemeClassifier());
  input.set_current_url(current_url);
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  autocomplete_controller_->StartPrefetch(input);
}
