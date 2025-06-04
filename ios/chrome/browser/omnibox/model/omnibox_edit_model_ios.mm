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
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_view_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_view_ios.h"
#import "net/cookies/cookie_util.h"
#import "third_party/icu/source/common/unicode/ubidi.h"
#import "third_party/metrics_proto/omnibox_event.pb.h"
#import "third_party/metrics_proto/omnibox_focus_type.pb.h"
#import "url/third_party/mozilla/url_parse.h"
#import "url/url_util.h"

using bookmarks::BookmarkModel;
using metrics::OmniboxEventProto;

// Helpers --------------------------------------------------------------------

namespace {

const char kOmniboxfocus_resulted_in_navigation[] =
    "Omnibox.focus_resulted_in_navigation";

}  // namespace

// OmniboxEditModelIOS
// -----------------------------------------------------------

OmniboxEditModelIOS::OmniboxEditModelIOS(OmniboxControllerIOS* controller,
                                         OmniboxViewIOS* view)
    : controller_(controller),
      view_(view),
      text_model_(std::make_unique<OmniboxTextModel>()) {}

OmniboxEditModelIOS::~OmniboxEditModelIOS() = default;

void OmniboxEditModelIOS::set_popup_view(OmniboxPopupViewIOS* popup_view) {
  popup_view_ = popup_view;
}

metrics::OmniboxEventProto::PageClassification
OmniboxEditModelIOS::GetPageClassification() const {
  return controller_->client()->GetPageClassification(/*is_prefetch=*/false);
}

AutocompleteMatch OmniboxEditModelIOS::CurrentMatch(
    GURL* alternate_nav_url) const {
  // If we have a valid match use it. Otherwise get one for the current text.
  AutocompleteMatch match = current_match_;
  if (!match.destination_url.is_valid()) {
    GetInfoForCurrentText(&match, alternate_nav_url);
  } else if (alternate_nav_url) {
    AutocompleteProviderClient* provider_client =
        autocomplete_controller()->autocomplete_provider_client();
    *alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
        text_model_->input, match, provider_client);
  }
  return match;
}

bool OmniboxEditModelIOS::ResetDisplayTexts() {
  const std::u16string old_display_text = GetPermanentDisplayText();
  url_for_editing_ = controller_->client()->GetFormattedFullURL();
  // When there's new permanent text, and the user isn't interacting with the
  // omnibox, we want to revert the edit to show the new text.  We could simply
  // define "interacting" as "the omnibox has focus", but we still allow updates
  // when the omnibox has focus as long as the user hasn't begun editing, and
  // isn't seeing zerosuggestions (because changing this text would require
  // changing or hiding those suggestions).  When the omnibox doesn't have
  // focus, we assume the user may have abandoned their interaction and it's
  // always safe to change the text; this also prevents someone toggling "Show
  // URL" (which sounds as if it might be persistent) from seeing just that URL
  // forever afterwards.
  return (GetPermanentDisplayText() != old_display_text) &&
         (!has_focus() ||
          (!text_model_->user_input_in_progress && !PopupIsOpen()));
}

std::u16string OmniboxEditModelIOS::GetPermanentDisplayText() const {
  return url_for_editing_;
}

void OmniboxEditModelIOS::SetUserText(const std::u16string& text) {
  SetInputInProgress(true);
  InternalSetUserText(text);
  GetInfoForCurrentText(&current_match_, nullptr);
  text_model_->paste_state = OmniboxPasteState::kNone;
}

void OmniboxEditModelIOS::OnChanged() {
  // Don't call CurrentMatch() when there's no editing, as in this case we'll
  // never actually use it.  This avoids running the autocomplete providers (and
  // any systems they then spin up) during startup.
  const AutocompleteMatch& current_match = text_model_->user_input_in_progress
                                               ? CurrentMatch(nullptr)
                                               : AutocompleteMatch();

  controller_->client()->OnTextChanged(
      current_match, text_model_->user_input_in_progress,
      text_model_->user_text, autocomplete_controller()->result(), has_focus());
}

bool OmniboxEditModelIOS::CurrentTextIsURL() const {
  // If !user_text_model_->inputin_progress_, we can determine if the text is a
  // URL without starting the autocomplete system. This speeds browser startup.
  return !text_model_->user_input_in_progress ||
         !AutocompleteMatch::IsSearchType(CurrentMatch(nullptr).type);
}

void OmniboxEditModelIOS::AdjustTextForCopy(int sel_min,
                                            std::u16string* text,
                                            GURL* url_from_text,
                                            bool* write_url) {
  omnibox::AdjustTextForCopy(
      sel_min, text,
      /*has_user_modified_text=*/text_model_->user_input_in_progress ||
          *text != url_for_editing_,
      /*is_keyword_selected=*/false,
      PopupIsOpen() ? std::optional<AutocompleteMatch>(CurrentMatch(nullptr))
                    : std::nullopt,
      controller_->client(), url_from_text, write_url);
}

void OmniboxEditModelIOS::UpdateInput(bool has_selected_text,
                                      bool prevent_inline_autocomplete) {
  bool changed_to_user_input_in_progress = SetInputInProgressNoNotify(true);
  if (!has_focus()) {
    if (changed_to_user_input_in_progress) {
      NotifyObserversInputInProgress(true);
    }
    return;
  }

  if (changed_to_user_input_in_progress && text_model_->user_text.empty()) {
    // In the case the user enters user-input-in-progress mode by clearing
    // everything (i.e. via Backspace), ask for ZeroSuggestions instead of the
    // normal prefix (as-you-type) autocomplete.
    //
    // The difference between a ZeroSuggest request and a normal
    // prefix autocomplete request is getting fuzzier, and should be fully
    // encapsulated by the AutocompleteInput::focus_type() member. We should
    // merge these two calls soon, lest we confuse future developers.
    StartZeroSuggestRequest(/*user_clobbered_permanent_text=*/true);
  } else {
    // Otherwise run the normal prefix (as-you-type) autocomplete.
    StartAutocomplete(has_selected_text, prevent_inline_autocomplete);
  }

  if (changed_to_user_input_in_progress) {
    NotifyObserversInputInProgress(true);
  }
}

void OmniboxEditModelIOS::SetInputInProgress(bool in_progress) {
  if (SetInputInProgressNoNotify(in_progress)) {
    NotifyObserversInputInProgress(in_progress);
  }
}

void OmniboxEditModelIOS::Revert() {
  SetInputInProgress(false);
  text_model_->input.Clear();
  text_model_->paste_state = OmniboxPasteState::kNone;
  InternalSetUserText(std::u16string());
  size_t start, end;
  if (view_) {
    view_->GetSelectionBounds(&start, &end);
  }
  current_match_ = AutocompleteMatch();
  // First home the cursor, so view of text is scrolled to left, then correct
  // it. `SetCaretPos()` doesn't scroll the text, so doing that first wouldn't
  // accomplish anything.
  std::u16string current_permanent_url = GetPermanentDisplayText();
  if (view_) {
    view_->SetWindowTextAndCaretPos(current_permanent_url, 0, false, true);
    view_->SetCaretPos(std::min(current_permanent_url.length(), start));
  }
  controller_->client()->OnRevert();
}

void OmniboxEditModelIOS::StartAutocomplete(bool has_selected_text,
                                            bool prevent_inline_autocomplete) {
  const std::u16string input_text = text_model_->user_text;

  size_t start, cursor_position;
  // This method currently only works when there's a view, but ideally the
  // model should be primary for determining such state.
  CHECK(view_);
  view_->GetSelectionBounds(&start, &cursor_position);

  text_model_->input = AutocompleteInput(
      input_text, cursor_position, GetPageClassification(),
      controller_->client()->GetSchemeClassifier(),
      controller_->client()->ShouldDefaultTypedNavigationsToHttps(),
      controller_->client()->GetHttpsPortForTesting(),
      controller_->client()->IsUsingFakeHttpsForHttpsUpgradeTesting());
  text_model_->input.set_current_url(controller_->client()->GetURL());
  text_model_->input.set_current_title(controller_->client()->GetTitle());
  text_model_->input.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete || text_model_->just_deleted_text ||
      (has_selected_text && text_model_->inline_autocompletion.empty()) ||
      text_model_->paste_state != OmniboxPasteState::kNone);
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
          controller_->client()->GetLensOverlaySuggestInputs()) {
    text_model_->input.set_lens_overlay_suggest_inputs(*suggest_inputs);
  }

  controller_->StartAutocomplete(text_model_->input);
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
  if (view_) {
    view_->SetAdditionalText(std::u16string());
  }
}

void OmniboxEditModelIOS::OnSetFocus() {
  TRACE_EVENT0("omnibox", "OmniboxEditModelIOS::OnSetFocus");
  text_model_->last_omnibox_focus = base::TimeTicks::Now();
  text_model_->focus_resulted_in_navigation = false;

  // If the omnibox lost focus while the caret was hidden and then regained
  // focus, OnSetFocus() is called and should restore visibility. Note that
  // focus can be regained without an accompanying call to
  // OmniboxViewIOS::SetFocus(), e.g. by tabbing in.
  SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);

  if (text_model_->user_input_in_progress || !text_model_->in_revert) {
    controller_->client()->OnInputStateChanged();
  }

  if (omnibox_feature_configs::HappinessTrackingSurveyForOmniboxOnFocusZps::
          Get()
              .enabled) {
    controller_->client()->MaybeShowOnFocusHatsSurvey(
        autocomplete_controller()->autocomplete_provider_client());
  }
}

void OmniboxEditModelIOS::StartZeroSuggestRequest(
    bool user_clobbered_permanent_text) {
  // Early exit if a query is already in progress or the popup is already open.
  // This is what allows this method to be called multiple times in multiple
  // code locations without harm.
  if (!autocomplete_controller()->done() || PopupIsOpen()) {
    return;
  }

  // Early exit if the page has not loaded yet, so we don't annoy users.
  if (!controller_->client()->CurrentPageExists()) {
    return;
  }

  // Early exit if the user already has a navigation or search query in mind.
  if (text_model_->user_input_in_progress && !user_clobbered_permanent_text) {
    return;
  }

  TRACE_EVENT0("omnibox", "OmniboxEditModelIOS::StartZeroSuggestRequest");

  // Send the textfield contents exactly as-is, as otherwise the verbatim
  // match can be wrong. The full page URL is anyways in set_current_url().
  // Don't attempt to use https as the default scheme for these requests.
  text_model_->input = AutocompleteInput(
      GetText(), GetPageClassification(),
      controller_->client()->GetSchemeClassifier(),
      /*should_use_https_as_default_scheme=*/false,
      controller_->client()->GetHttpsPortForTesting(),
      controller_->client()->IsUsingFakeHttpsForHttpsUpgradeTesting());
  text_model_->input.set_current_url(controller_->client()->GetURL());
  text_model_->input.set_current_title(controller_->client()->GetTitle());
  text_model_->input.set_focus_type(
      metrics::OmniboxFocusType::INTERACTION_FOCUS);
  // Set the lens overlay suggest inputs, if available.
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
          controller_->client()->GetLensOverlaySuggestInputs()) {
    text_model_->input.set_lens_overlay_suggest_inputs(*suggest_inputs);
  }
  controller_->StartAutocomplete(text_model_->input);
}

void OmniboxEditModelIOS::OnWillKillFocus() {
  if (text_model_->user_input_in_progress || !text_model_->in_revert) {
    controller_->client()->OnInputStateChanged();
  }
}

void OmniboxEditModelIOS::OnKillFocus() {
  UMA_HISTOGRAM_BOOLEAN(kOmniboxfocus_resulted_in_navigation,
                        text_model_->focus_resulted_in_navigation);
  SetFocusState(OMNIBOX_FOCUS_NONE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  text_model_->last_omnibox_focus = base::TimeTicks();
  text_model_->paste_state = OmniboxPasteState::kNone;
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

void OmniboxEditModelIOS::OnPopupDataChanged(
    const std::u16string& inline_autocompletion,
    const std::u16string& additional_text,
    const AutocompleteMatch& new_match) {
  current_match_ = new_match;

  text_model_->inline_autocompletion = inline_autocompletion;

  const std::u16string& user_text = text_model_->user_input_in_progress
                                        ? text_model_->user_text
                                        : text_model_->input.text();

  if (view_) {
    view_->OnInlineAutocompleteTextMaybeChanged(
        user_text, text_model_->inline_autocompletion);
    view_->SetAdditionalText(additional_text);
  }
  // We need to invoke OnChanged in case the destination url changed (as could
  // happen when control is toggled).
  OnChanged();
}

bool OmniboxEditModelIOS::OnAfterPossibleChange(
    const OmniboxViewIOS::StateChanges& state_changes) {
  // Update the paste state as appropriate: if we're just finishing a paste
  // that replaced all the text, preserve that information; otherwise, if we've
  // made some other edit, clear paste tracking.
  if (text_model_->paste_state == OmniboxPasteState::kPasting) {
    text_model_->paste_state = OmniboxPasteState::kPasted;

    GURL url = GURL(*(state_changes.new_text));
    if (url.is_valid()) {
      controller_->client()->OnUserPastedInOmniboxResultingInValidURL();
    }
  } else if (state_changes.text_differs) {
    text_model_->paste_state = OmniboxPasteState::kNone;
  }

  if (state_changes.text_differs || state_changes.selection_differs) {
    // Restore caret visibility whenever the user changes text or selection in
    // the omnibox.
    SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_TYPING);
  }

  // If the user text does not need to be changed, return now, so we don't
  // change any other state, lest arrowing around the omnibox do something like
  // reset `just_deleted_text_`.  Note that modifying the selection accepts any
  // inline autocompletion, which results in a user text change.
  if (!state_changes.text_differs &&
      (!state_changes.selection_differs ||
       text_model_->inline_autocompletion.empty())) {
    return false;
  }

  InternalSetUserText(*state_changes.new_text);
  text_model_->just_deleted_text = state_changes.just_deleted_text;

  if (view_) {
    view_->UpdatePopup();
  }

  return true;
}

// static
const char OmniboxEditModelIOS::kCutOrCopyAllTextHistogram[] =
    "Omnibox.CutOrCopyAllText";

void OmniboxEditModelIOS::InternalSetUserText(const std::u16string& text) {
  text_model_->user_text = text;
  text_model_->just_deleted_text = false;
  text_model_->inline_autocompletion.clear();
}

void OmniboxEditModelIOS::GetInfoForCurrentText(AutocompleteMatch* match,
                                                GURL* alternate_nav_url) const {
  DCHECK(match);

  // If there's a query in progress or the popup is open, pick out the default
  // match or selected match, if there is one.
  bool found_match_for_text = false;
  if (!autocomplete_controller()->done() || PopupIsOpen()) {
    if (!autocomplete_controller()->done() &&
        autocomplete_controller()->result().default_match()) {
      // The user cannot have manually selected a match, or the query would have
      // stopped. So the default match must be the desired selection.
      *match = *autocomplete_controller()->result().default_match();
      found_match_for_text = true;
    }
    if (found_match_for_text && alternate_nav_url) {
      AutocompleteProviderClient* provider_client =
          autocomplete_controller()->autocomplete_provider_client();
      *alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
          text_model_->input, *match, provider_client);
    }
  }

  if (!found_match_for_text) {
    // For match generation, we use the unelided `url_for_editing_`, unless the
    // user input is in progress.
    std::u16string text_for_match_generation =
        text_model_->user_input_in_progress ? text_model_->user_text
                                            : url_for_editing_;

    controller_->client()->GetAutocompleteClassifier()->Classify(
        text_for_match_generation, false, true, GetPageClassification(), match,
        alternate_nav_url);
  }
}

bool OmniboxEditModelIOS::PopupIsOpen() const {
  return popup_view_ && popup_view_->IsOpen();
}

void OmniboxEditModelIOS::SetAutocompleteInput(AutocompleteInput input) {
  text_model_->input = std::move(input);
}

PrefService* OmniboxEditModelIOS::GetPrefService() {
  return controller_->client()->GetPrefs();
}

const PrefService* OmniboxEditModelIOS::GetPrefService() const {
  return controller_->client()->GetPrefs();
}

AutocompleteController* OmniboxEditModelIOS::autocomplete_controller() const {
  return controller_->autocomplete_controller();
}

void OmniboxEditModelIOS::AcceptInput(
    WindowOpenDisposition disposition,
    base::TimeTicks match_selection_timestamp) {
  // Get the URL and transition type for the selected entry.
  GURL alternate_nav_url;
  AutocompleteMatch match = CurrentMatch(&alternate_nav_url);

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

  if (popup_view_) {
    OpenMatch(OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch), match,
              disposition, alternate_nav_url, std::u16string(),
              match_selection_timestamp);
  }
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
  const base::TimeTicks& now(base::TimeTicks::Now());
  base::TimeDelta elapsed_time_since_user_first_modified_omnibox(
      now - text_model_->time_user_first_modified_omnibox);
  autocomplete_controller()
      ->UpdateMatchDestinationURLWithAdditionalSearchboxStats(
          elapsed_time_since_user_first_modified_omnibox, &match);

  GURL destination_url = action ? action->getUrl() : match.destination_url;

  // Save the result of the interaction, but do not record the histogram yet.
  text_model_->focus_resulted_in_navigation = true;

  omnibox::RecordActionShownForAllActions(autocomplete_controller()->result(),
                                          selection);
  HistoryFuzzyProvider::RecordOpenMatchMetrics(
      autocomplete_controller()->result(), match);

  std::u16string input_text(pasted_text);
  if (input_text.empty()) {
    input_text = text_model_->user_input_in_progress ? text_model_->user_text
                                                     : url_for_editing_;
  }
  // Create a dummy AutocompleteInput for use in calling VerbatimMatchForInput()
  // to create an alternate navigational match.
  AutocompleteInput alternate_input(
      input_text, GetPageClassification(),
      controller_->client()->GetSchemeClassifier(),
      controller_->client()->ShouldDefaultTypedNavigationsToHttps(), 0, false);
  // Somehow we can occasionally get here with no active tab.  It's not
  // clear why this happens.
  alternate_input.set_current_url(controller_->client()->GetURL());
  alternate_input.set_current_title(controller_->client()->GetTitle());

  base::TimeDelta elapsed_time_since_last_change_to_default_match(
      now - autocomplete_controller()->last_time_default_match_changed());
  DCHECK(match.provider);
  // These elapsed times don't really make sense for matches that come from
  // omnibox focus (because the user did not modify the omnibox), so for those
  // we set the elapsed times to something that will be ignored by
  // metrics_log.cc.  They also don't necessarily make sense if the omnibox
  // dropdown is closed or the user used paste-and-go.  (In most
  // cases when this happens, the user never modified the omnibox.)
  const bool popup_open = PopupIsOpen();
  const base::TimeDelta default_time_delta = base::Milliseconds(-1);
  if (text_model_->input.IsZeroSuggest() || !pasted_text.empty()) {
    elapsed_time_since_user_first_modified_omnibox = default_time_delta;
    elapsed_time_since_last_change_to_default_match = default_time_delta;
  }

  base::TimeDelta elapsed_time_since_user_focused_omnibox = default_time_delta;
  if (!text_model_->last_omnibox_focus.is_null()) {
    elapsed_time_since_user_focused_omnibox =
        now - text_model_->last_omnibox_focus;
    // Only record focus to open time when a focus actually happened (as
    // opposed to, say, dragging a link onto the omnibox).

    omnibox::LogFocusToOpenTime(
        elapsed_time_since_user_focused_omnibox,
        text_model_->input.IsZeroSuggest(), GetPageClassification(), match,
        selection.IsAction() ? selection.action_index : -1);
  }

  // In some unusual cases, we ignore autocomplete_controller()->result() and
  // instead log a fake result set with a single element (`match`) and
  // selected_index of 0. For these cases:
  //  1. If the popup is closed (there is no result set). This doesn't apply
  //  for WebUI searchboxes since they don't have an associated popup.
  //  2. If the index is out of bounds. This should only happen if
  //  `selection.line` is
  //     kNoMatch, which can happen if the default search provider is disabled.
  //  3. If this is paste-and-go (meaning the contents of the dropdown
  //     are ignored regardless).
  const bool dropdown_ignored =
      (!popup_open && !omnibox::IsWebUISearchbox(GetPageClassification())) ||
      selection.line >= autocomplete_controller()->result().size() ||
      !pasted_text.empty();
  ACMatches fake_single_entry_matches;
  fake_single_entry_matches.push_back(match);
  AutocompleteResult fake_single_entry_result;
  fake_single_entry_result.AppendMatches(fake_single_entry_matches);

  std::u16string user_text =
      text_model_->input.IsZeroSuggest() ? std::u16string() : input_text;
  size_t completed_length = match.allowed_to_be_default_match
                                ? match.inline_autocompletion.length()
                                : std::u16string::npos;
  bool is_incognito = autocomplete_controller()
                          ->autocomplete_provider_client()
                          ->IsOffTheRecord();
  bool contextual_search_selected =
      match.takeover_action &&
      match.takeover_action->ActionId() ==
          OmniboxActionId::CONTEXTUAL_SEARCH_FULFILLMENT;
  bool lens_action_selected =
      match.takeover_action && match.takeover_action->ActionId() ==
                                   OmniboxActionId::CONTEXTUAL_SEARCH_OPEN_LENS;
  OmniboxLog log(
      user_text, text_model_->just_deleted_text, text_model_->input.type(),
      /*is_keyword_selected=*/false, OmniboxEventProto::INVALID, popup_open,
      dropdown_ignored ? OmniboxPopupSelection(0) : selection, disposition,
      !pasted_text.empty(),
      SessionID::InvalidValue(),  // don't know tab ID; set later if appropriate
      GetPageClassification(), elapsed_time_since_user_first_modified_omnibox,
      completed_length, elapsed_time_since_last_change_to_default_match,
      dropdown_ignored ? fake_single_entry_result
                       : autocomplete_controller()->result(),
      destination_url, is_incognito,
      match.zero_prefix_suggestions_shown_in_session,
      match.zero_prefix_search_suggestions_shown_in_session,
      match.zero_prefix_url_suggestions_shown_in_session,
      match.typed_search_suggestions_shown_in_session,
      match.typed_url_suggestions_shown_in_session, contextual_search_selected,
      match.contextual_search_suggestions_shown_in_session,
      lens_action_selected, match.lens_action_shown_in_session);
// Check disabled on iOS as the platform shows a default suggestion on focus
// (crbug.com/40061502).
#if !BUILDFLAG(IS_IOS)
  DCHECK(dropdown_ignored ||
         (log.elapsed_time_since_user_first_modified_omnibox >=
          log.elapsed_time_since_last_change_to_default_match))
      << "We should've got the notification that the user modified the "
      << "omnibox text at same time or before the most recent time the "
      << "default match changed.";
#endif
  log.elapsed_time_since_user_focused_omnibox =
      elapsed_time_since_user_focused_omnibox;
  log.ukm_source_id = controller_->client()->GetUKMSourceId();

  if ((disposition == WindowOpenDisposition::CURRENT_TAB) &&
      controller_->client()->CurrentPageExists()) {
    // If we know the destination is being opened in the current tab,
    // we can easily get the tab ID.  (If it's being opened in a new
    // tab, we don't know the tab ID yet.)
    log.tab_id = controller_->client()->GetSessionID();
  }
  autocomplete_controller()->AddProviderAndTriggeringLogs(&log);

  base::UmaHistogramEnumeration("Omnibox.SuggestionUsed.RichAutocompletion",
                                match.rich_autocompletion_triggered);

  omnibox::LogIPv4PartsCount(user_text, destination_url, completed_length);

  controller_->client()->OnURLOpenedFromOmnibox(&log);
  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);
  omnibox::answer_data_parser::LogAnswerUsed(match.answer_type);

  TemplateURLService* service = controller_->client()->GetTemplateURLService();
  TemplateURL* template_url = match.GetTemplateURL(service, false);
  if (template_url) {
    // `match` is a Search navigation; log search engine usage metrics.
    AutocompleteMatch::LogSearchEngineUsed(match, service);

      DCHECK(ui::PageTransitionTypeIncludingQualifiersIs(
                 match.transition, ui::PAGE_TRANSITION_GENERATED) ||
             ui::PageTransitionTypeIncludingQualifiersIs(
                 match.transition, ui::PAGE_TRANSITION_RELOAD));
  } else {
    // `match` is a URL navigation, not a search.
    // For logging the below histogram, only record uses that depend on the
    // omnibox suggestion system, i.e., TYPED navigations.  That is, exclude
    // omnibox URL interactions that are treated as reloads or link-following
    // (i.e., cut-and-paste of URLs) or paste-and-go.
    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_TYPED) &&
        pasted_text.empty()) {
      navigation_metrics::RecordOmniboxURLNavigation(destination_url);
    }

    // The following histograms should be recorded for both TYPED and pasted
    // URLs, but should still exclude reloads.
    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_TYPED) ||
        ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                    ui::PAGE_TRANSITION_LINK)) {
      net::cookie_util::RecordCookiePortOmniboxHistograms(destination_url);
    }
  }

  if (action) {
    OmniboxAction::ExecutionContext context(
        *(autocomplete_controller()->autocomplete_provider_client()),
        base::BindOnce(&OmniboxClient::OnAutocompleteAccept,
                       controller_->client()->AsWeakPtr()),
        match_selection_timestamp, disposition);
    action->Execute(context);
  }

  if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB && view_) {
    base::AutoReset<bool> tmp(&text_model_->in_revert, true);
    view_->RevertAll();  // Revert the box to its unedited state.
  }

  if (!action) {
    // Track whether the destination URL sends us to a search results page
    // using the default search provider.
    TemplateURLService* template_url_service =
        controller_->client()->GetTemplateURLService();
    if (template_url_service &&
        template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
            match.destination_url)) {
      base::RecordAction(
          base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
      base::UmaHistogramBoolean("Omnibox.Search.OffTheRecord", is_incognito);
    }

    BookmarkModel* bookmark_model = controller_->client()->GetBookmarkModel();
    if (bookmark_model && bookmark_model->IsBookmarked(destination_url)) {
      controller_->client()->OnBookmarkLaunched();
    }

    // This block should be the last call in OpenMatch, because controller_ is
    // not guaranteed to exist after client()->OnAutocompleteAccept is called.
    if (destination_url.is_valid()) {
      // This calls RevertAll again.
      base::AutoReset<bool> tmp(&text_model_->in_revert, true);

      controller_->client()->OnAutocompleteAccept(
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

bool OmniboxEditModelIOS::SetInputInProgressNoNotify(bool in_progress) {
  if (text_model_->user_input_in_progress == in_progress) {
    return false;
  }

  text_model_->user_input_in_progress = in_progress;
  if (text_model_->user_input_in_progress) {
    text_model_->time_user_first_modified_omnibox = base::TimeTicks::Now();
    base::RecordAction(base::UserMetricsAction("OmniboxInputInProgress"));
    autocomplete_controller()->ResetSession();
  }
  return true;
}

void OmniboxEditModelIOS::NotifyObserversInputInProgress(bool in_progress) {
  controller_->client()->OnInputInProgress(in_progress);

  if (text_model_->user_input_in_progress || !text_model_->in_revert) {
    controller_->client()->OnInputStateChanged();
  }
}

void OmniboxEditModelIOS::SetFocusState(OmniboxFocusState state,
                                        OmniboxFocusChangeReason reason) {
  if (state == text_model_->focus_state) {
    return;
  }

  text_model_->focus_state = state;
  controller_->client()->OnFocusChanged(text_model_->focus_state, reason);
}

std::u16string OmniboxEditModelIOS::GetText() const {
  // Once the model owns primary text, the check for `view_` won't be needed.
  if (view_) {
    return view_->GetText();
  } else {
    NOTREACHED();
  }
}
