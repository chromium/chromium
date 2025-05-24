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
  // Directly observe omnibox's `AutocompleteController` instance - i.e., when
  // `view` is provided in the constructor. In the case of realbox - i.e., when
  // `view` is not provided in the constructor - `RealboxHandler` directly
  // observes the `AutocompleteController` instance itself.
  if (view) {
    autocomplete_controller_->AddObserver(this);
  }

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

void OmniboxControllerIOS::OnResultChanged(AutocompleteController* controller,
                                           bool default_match_changed) {
  TRACE_EVENT0("omnibox", "OmniboxControllerIOS::OnResultChanged");
  DCHECK(controller == autocomplete_controller_.get());

  const bool popup_was_open = edit_model_->PopupIsOpen();
  if (default_match_changed) {
    // The default match has changed, we need to let the OmniboxEditModelIOS
    // know about new inline autocomplete text (blue highlight).
    if (autocomplete_controller_->result().default_match()) {
      edit_model_->OnCurrentMatchChanged();
    } else {
      edit_model_->OnPopupResultChanged();
      edit_model_->OnPopupDataChanged(std::u16string(), std::u16string(),
                                      AutocompleteMatch());
    }
  } else {
    edit_model_->OnPopupResultChanged();
  }

  const bool popup_is_open = edit_model_->PopupIsOpen();
  if (popup_was_open != popup_is_open) {
    client_->OnPopupVisibilityChanged(popup_is_open);
  }

  if (popup_was_open && !popup_is_open) {
    // Closing the popup can change the default suggestion. This usually occurs
    // when it's unclear whether the input represents a search or URL; e.g.,
    // 'a.com/b c' or when title autocompleting. Clear the additional text to
    // avoid suggesting the omnibox contains a URL suggestion when that may no
    // longer be the case; i.e. when the default suggestion changed from a URL
    // to a search suggestion upon closing the popup.
    edit_model_->ClearAdditionalText();
  }

  // Note: The client outlives `this`, so bind a weak pointer to the callback
  // passed in to eliminate the potential for crashes on shutdown.
  // `should_preload` is set to `controller->done()` as prerender may only want
  // to start preloading a result after all Autocomplete results are ready.
  client_->OnResultChanged(autocomplete_controller_->result(),
                           default_match_changed,
                           /*should_preload=*/controller->done(),
                           /*on_bitmap_fetched=*/base::DoNothing());
}
