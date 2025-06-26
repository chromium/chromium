// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"

#import <algorithm>
#import <iterator>
#import <memory>
#import <string>
#import <string_view>
#import <utility>

#import "base/auto_reset.h"
#import "base/feature_list.h"
#import "base/format_macros.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/strcat.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/trace_event/trace_event.h"
#import "base/trace_event/typed_macros.h"
#import "build/branding_buildflags.h"
#import "build/build_config.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/dom_distiller/core/url_constants.h"
#import "components/dom_distiller/core/url_utils.h"
#import "components/history_embeddings/history_embeddings_features.h"
#import "components/navigation_metrics/navigation_metrics.h"
#import "components/omnibox/browser/actions/omnibox_action.h"
#import "components/omnibox/browser/actions/omnibox_pedal.h"
#import "components/omnibox/browser/actions/omnibox_pedal_concepts.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_type.h"
#import "components/omnibox/browser/autocomplete_provider.h"
#import "components/omnibox/browser/autocomplete_provider_client.h"
#import "components/omnibox/browser/history_fuzzy_provider.h"
#import "components/omnibox/browser/history_url_provider.h"
#import "components/omnibox/browser/keyword_provider.h"
#import "components/omnibox/browser/omnibox.mojom-shared.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_event_global_tracker.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/browser/omnibox_log.h"
#import "components/omnibox/browser/omnibox_logging_utils.h"
#import "components/omnibox/browser/omnibox_metrics_provider.h"
#import "components/omnibox/browser/omnibox_navigation_observer.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/omnibox/browser/omnibox_prefs.h"
#import "components/omnibox/browser/omnibox_text_util.h"
#import "components/omnibox/browser/page_classification_functions.h"
#import "components/omnibox/browser/search_provider.h"
#import "components/omnibox/browser/suggestion_answer.h"
#import "components/omnibox/browser/verbatim_match.h"
#import "components/omnibox/common/omnibox_feature_configs.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/search_engine_type.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_starter_pack_data.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "net/cookies/cookie_util.h"
#import "third_party/icu/source/common/unicode/ubidi.h"
#import "third_party/metrics_proto/omnibox_event.pb.h"
#import "third_party/metrics_proto/omnibox_focus_type.pb.h"
#import "url/third_party/mozilla/url_parse.h"
#import "url/url_util.h"

using bookmarks::BookmarkModel;
using metrics::OmniboxEventProto;

OmniboxEditModelIOS::OmniboxEditModelIOS(
    OmniboxControllerIOS* controller,
    OmniboxClient* client,
    OmniboxTextModel* text_model,
    OmniboxMetricsRecorder* metrics_recorder)
    : controller_(controller),
      client_(client),
      text_model_(text_model),
      metrics_recorder_(metrics_recorder) {}

OmniboxEditModelIOS::~OmniboxEditModelIOS() = default;

void OmniboxEditModelIOS::set_text_controller(
    OmniboxTextController* text_controller) {
  text_controller_ = text_controller;
}

metrics::OmniboxEventProto::PageClassification
OmniboxEditModelIOS::GetPageClassification() const {
  return client_->GetPageClassification(/*is_prefetch=*/false);
}

bool OmniboxEditModelIOS::CurrentTextIsURL() const {
  // If !user_text_model_->inputin_progress_, we can determine if the text is a
  // URL without starting the autocomplete system. This speeds browser startup.
  return !text_model_->user_input_in_progress ||
         !AutocompleteMatch::IsSearchType(
             [text_controller_ currentMatch:nullptr].type);
}

void OmniboxEditModelIOS::AdjustTextForCopy(int sel_min,
                                            std::u16string* text,
                                            GURL* url_from_text,
                                            bool* write_url) {
  omnibox::AdjustTextForCopy(
      sel_min, text,
      /*has_user_modified_text=*/text_model_->user_input_in_progress ||
          *text != text_model_->url_for_editing,
      /*is_keyword_selected=*/false,
      PopupIsOpen() ? std::optional<AutocompleteMatch>(
                          [text_controller_ currentMatch:nullptr])
                    : std::nullopt,
      client_, url_from_text, write_url);
}

void OmniboxEditModelIOS::Revert() {
  [text_controller_ revertState];
}

void OmniboxEditModelIOS::OpenSelection(OmniboxPopupSelection selection,
                                        base::TimeTicks timestamp,
                                        WindowOpenDisposition disposition) {
  // Intentionally accept input when selection has no line.
  // This will usually reach `OpenMatch` indirectly.
  if (selection.line >= autocomplete_controller()->result().size()) {
    AcceptInput(disposition, timestamp);
    return;
  }

  const AutocompleteMatch& match =
      autocomplete_controller()->result().match_at(selection.line);

  // Open the match.
  GURL alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
      text_model_->input, match,
      autocomplete_controller()->autocomplete_provider_client());
  OpenMatch(selection, match, disposition, alternate_nav_url, std::u16string(),
            timestamp);
}

void OmniboxEditModelIOS::OpenSelection(base::TimeTicks timestamp,
                                        WindowOpenDisposition disposition) {
  OpenSelection(OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                                      OmniboxPopupSelection::NORMAL),
                timestamp, disposition);
}

void OmniboxEditModelIOS::ClearAdditionalText() {
  TRACE_EVENT0("omnibox", "OmniboxEditModelIOS::ClearAdditionalText");
  [text_controller_ setAdditionalText:std::u16string()];
}

void OmniboxEditModelIOS::OnSetFocus() {
  text_model_->OnSetFocus();
}

void OmniboxEditModelIOS::OnPaste() {
  UMA_HISTOGRAM_COUNTS_1M("Omnibox.Paste", 1);
  text_model_->paste_state = OmniboxPasteState::kPasting;
}

void OmniboxEditModelIOS::OpenMatchForTesting(
    AutocompleteMatch match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t index,
    base::TimeTicks match_selection_timestamp) {
  OpenMatch(OmniboxPopupSelection(index), match, disposition, alternate_nav_url,
            pasted_text, match_selection_timestamp);
}

bool OmniboxEditModelIOS::PopupIsOpen() const {
  return omnibox_autocomplete_controller_.hasSuggestions;
}

void OmniboxEditModelIOS::SetAutocompleteInput(AutocompleteInput input) {
  text_model_->input = std::move(input);
}

PrefService* OmniboxEditModelIOS::GetPrefService() {
  return client_->GetPrefs();
}

const PrefService* OmniboxEditModelIOS::GetPrefService() const {
  return client_->GetPrefs();
}

AutocompleteController* OmniboxEditModelIOS::autocomplete_controller() const {
  return controller_->autocomplete_controller();
}

void OmniboxEditModelIOS::AcceptInput(
    WindowOpenDisposition disposition,
    base::TimeTicks match_selection_timestamp) {
  // Get the URL and transition type for the selected entry.
  GURL alternate_nav_url;
  AutocompleteMatch match = [text_controller_ currentMatch:&alternate_nav_url];

  if (!match.destination_url.is_valid()) {
    return;
  }

  if (text_model_->paste_state != OmniboxPasteState::kNone &&
      match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED) {
    // When the user pasted in a URL and hit enter, score it like a link click
    // rather than a normal typed URL, so it doesn't get inline autocompleted
    // as aggressively later.
    match.transition = ui::PAGE_TRANSITION_LINK;
  }

  OpenMatch(OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch), match,
            disposition, alternate_nav_url, std::u16string(),
            match_selection_timestamp);
}

void OmniboxEditModelIOS::OpenMatch(OmniboxPopupSelection selection,
                                    AutocompleteMatch match,
                                    WindowOpenDisposition disposition,
                                    const GURL& alternate_nav_url,
                                    const std::u16string& pasted_text,
                                    base::TimeTicks match_selection_timestamp) {
  // If the user is executing an action, this will be non-null and some match
  // opening and metrics behavior will be adjusted accordingly.
  OmniboxAction* action = nullptr;
  if (selection.state == OmniboxPopupSelection::NORMAL &&
      match.takeover_action) {
    DCHECK(match_selection_timestamp != base::TimeTicks());
    action = match.takeover_action.get();
  } else if (selection.IsAction()) {
    DCHECK_LT(selection.action_index, match.actions.size());
    action = match.actions[selection.action_index].get();
  }

  // Invalid URLs such as chrome://history can end up here, but that's okay
  // if the user is executing an action instead of navigating to the URL.
  if (!match.destination_url.is_valid() && !action) {
    return;
  }

  // NULL_RESULT_MESSAGE matches are informational only and cannot be acted
  // upon. Immediately return when attempting to open one.
  if (match.type == AutocompleteMatchType::NULL_RESULT_MESSAGE) {
    return;
  }

  // Also switch the window disposition for tab switch actions. The action
  // itself will already open with SWITCH_TO_TAB disposition, but the change
  // is needed earlier for metrics.
  bool is_tab_switch_action =
      action && action->ActionId() == OmniboxActionId::TAB_SWITCH;
  if (is_tab_switch_action) {
    disposition = WindowOpenDisposition::SWITCH_TO_TAB;
  }

  TRACE_EVENT("omnibox", "OmniboxEditModelIOS::OpenMatch", "match", match,
              "disposition", disposition, "altenate_nav_url", alternate_nav_url,
              "pasted_text", pasted_text);

  GURL destination_url = action ? action->getUrl() : match.destination_url;

  std::u16string input_text(pasted_text);
  if (input_text.empty()) {
    input_text = text_model_->user_input_in_progress
                     ? text_model_->user_text
                     : text_model_->url_for_editing;
  }

  // Save the result of the interaction, but do not record the histogram yet.
  text_model_->focus_resulted_in_navigation = true;

  // Create a dummy AutocompleteInput for use in calling VerbatimMatchForInput()
  // to create an alternate navigational match.
  AutocompleteInput alternate_input(
      input_text, GetPageClassification(), client_->GetSchemeClassifier(),
      client_->ShouldDefaultTypedNavigationsToHttps(), 0, false);
  // Somehow we can occasionally get here with no active tab.  It's not
  // clear why this happens.
  alternate_input.set_current_url(client_->GetURL());
  alternate_input.set_current_title(client_->GetTitle());

  [metrics_recorder_ recordOpenMatch:match
                      destinationURL:destination_url
                           inputText:input_text
                      popupSelection:selection
               windowOpenDisposition:disposition
                            isAction:action
                        isPastedText:!pasted_text.empty()];

  if (action) {
    OmniboxAction::ExecutionContext context(
        *(autocomplete_controller()->autocomplete_provider_client()),
        base::BindOnce(&OmniboxClient::OnAutocompleteAccept,
                       client_->AsWeakPtr()),
        match_selection_timestamp, disposition);
    action->Execute(context);
  }

  if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    base::AutoReset<bool> tmp(&text_model_->in_revert, true);
    [text_controller_ revertAll];  // Revert the box to its unedited state.
  }

  if (!action) {
    // This block should be the last call in OpenMatch, because controller_ is
    // not guaranteed to exist after client()->OnAutocompleteAccept is called.
    if (destination_url.is_valid()) {
      // This calls RevertAll again.
      base::AutoReset<bool> tmp(&text_model_->in_revert, true);

      client_->OnAutocompleteAccept(
          destination_url, match.post_content.get(), disposition,
          ui::PageTransitionFromInt(match.transition |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          match.type, match_selection_timestamp,
          text_model_->input.added_default_scheme_to_typed_url(),
          text_model_->input.typed_url_had_http_scheme() &&
              match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED,
          input_text, match,
          VerbatimMatchForInput(
              autocomplete_controller()->history_url_provider(),
              autocomplete_controller()->autocomplete_provider_client(),
              alternate_input, alternate_nav_url, false));
    }
  }
}

std::u16string OmniboxEditModelIOS::GetText() const {
  return [text_controller_ displayedText];
}
