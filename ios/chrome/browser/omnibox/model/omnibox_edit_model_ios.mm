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

OmniboxEditModelIOS::OmniboxEditModelIOS(OmniboxControllerIOS* controller,
                                         OmniboxClient* client,
                                         OmniboxTextModel* text_model)
    : controller_(controller), client_(client), text_model_(text_model) {}

OmniboxEditModelIOS::~OmniboxEditModelIOS() = default;

void OmniboxEditModelIOS::set_text_controller(
    OmniboxTextController* text_controller) {
  text_controller_ = text_controller;
}

metrics::OmniboxEventProto::PageClassification
OmniboxEditModelIOS::GetPageClassification() const {
  return client_->GetPageClassification(/*is_prefetch=*/false);
}

AutocompleteMatch OmniboxEditModelIOS::CurrentMatch(
    GURL* alternate_nav_url) const {
  // If we have a valid match use it. Otherwise get one for the current text.
  AutocompleteMatch match = text_model_->current_match;
  if (!match.destination_url.is_valid()) {
    [text_controller_ getInfoForCurrentText:&match
                     alternateNavigationURL:alternate_nav_url];
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
  text_model_->url_for_editing = client_->GetFormattedFullURL();
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
  return text_model_->url_for_editing;
}

void OmniboxEditModelIOS::OnChanged() {
  // Don't call CurrentMatch() when there's no editing, as in this case we'll
  // never actually use it.  This avoids running the autocomplete providers (and
  // any systems they then spin up) during startup.
  const AutocompleteMatch& current_match = text_model_->user_input_in_progress
                                               ? CurrentMatch(nullptr)
                                               : AutocompleteMatch();

  client_->OnTextChanged(current_match, text_model_->user_input_in_progress,
                         text_model_->user_text,
                         autocomplete_controller()->result(), has_focus());
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
          *text != text_model_->url_for_editing,
      /*is_keyword_selected=*/false,
      PopupIsOpen() ? std::optional<AutocompleteMatch>(CurrentMatch(nullptr))
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

void OmniboxEditModelIOS::OnPopupDataChanged(
    const std::u16string& inline_autocompletion,
    const std::u16string& additional_text,
    const AutocompleteMatch& new_match) {
  text_model_->current_match = new_match;
  text_model_->inline_autocompletion = inline_autocompletion;

  const std::u16string& user_text = text_model_->user_input_in_progress
                                        ? text_model_->user_text
                                        : text_model_->input.text();

  [text_controller_
      updateAutocompleteIfTextChanged:user_text
                       autocompletion:text_model_->inline_autocompletion];
  [text_controller_ setAdditionalText:additional_text];

  // We need to invoke OnChanged in case the destination url changed (as could
  // happen when control is toggled).
  OnChanged();
}

bool OmniboxEditModelIOS::OnAfterPossibleChange(
    const OmniboxStateChanges& state_changes) {
  bool state_changed =
      text_model_->UpdateStateAfterPossibleChange(state_changes);

  if (!state_changed) {
    return false;
  }

  [text_controller_ startAutocompleteAfterEdit];

  return true;
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
    input_text = text_model_->user_input_in_progress
                     ? text_model_->user_text
                     : text_model_->url_for_editing;
  }
  // Create a dummy AutocompleteInput for use in calling VerbatimMatchForInput()
  // to create an alternate navigational match.
  AutocompleteInput alternate_input(
      input_text, GetPageClassification(), client_->GetSchemeClassifier(),
      client_->ShouldDefaultTypedNavigationsToHttps(), 0, false);
  // Somehow we can occasionally get here with no active tab.  It's not
  // clear why this happens.
  alternate_input.set_current_url(client_->GetURL());
  alternate_input.set_current_title(client_->GetTitle());

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
      destination_url, is_incognito, text_model_->input.IsZeroSuggest(),
      match.session);
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
  log.ukm_source_id = client_->GetUKMSourceId();

  if ((disposition == WindowOpenDisposition::CURRENT_TAB) &&
      client_->CurrentPageExists()) {
    // If we know the destination is being opened in the current tab,
    // we can easily get the tab ID.  (If it's being opened in a new
    // tab, we don't know the tab ID yet.)
    log.tab_id = client_->GetSessionID();
  }
  autocomplete_controller()->AddProviderAndTriggeringLogs(&log);

  base::UmaHistogramEnumeration("Omnibox.SuggestionUsed.RichAutocompletion",
                                match.rich_autocompletion_triggered);

  omnibox::LogIPv4PartsCount(user_text, destination_url, completed_length);

  client_->OnURLOpenedFromOmnibox(&log);
  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);
  omnibox::answer_data_parser::LogAnswerUsed(match.answer_type);

  TemplateURLService* service = client_->GetTemplateURLService();
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
                       client_->AsWeakPtr()),
        match_selection_timestamp, disposition);
    action->Execute(context);
  }

  if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    base::AutoReset<bool> tmp(&text_model_->in_revert, true);
    [text_controller_ revertAll];  // Revert the box to its unedited state.
  }

  if (!action) {
    // Track whether the destination URL sends us to a search results page
    // using the default search provider.
    TemplateURLService* template_url_service = client_->GetTemplateURLService();
    if (template_url_service &&
        template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
            match.destination_url)) {
      base::RecordAction(
          base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
      base::UmaHistogramBoolean("Omnibox.Search.OffTheRecord", is_incognito);
    }

    BookmarkModel* bookmark_model = client_->GetBookmarkModel();
    if (bookmark_model && bookmark_model->IsBookmarked(destination_url)) {
      client_->OnBookmarkLaunched();
    }

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
