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
#import "components/omnibox/browser/omnibox_metrics_provider.h"
#import "components/omnibox/browser/omnibox_navigation_observer.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/omnibox/browser/omnibox_prefs.h"
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
#import "ios/chrome/browser/omnibox/model/omnibox_popup_view_base.h"
#import "ios/chrome/browser/omnibox/model/omnibox_view_base.h"
#import "net/cookies/cookie_util.h"
#import "third_party/icu/source/common/unicode/ubidi.h"
#import "third_party/metrics_proto/omnibox_event.pb.h"
#import "third_party/metrics_proto/omnibox_focus_type.pb.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/color_palette.h"
#import "ui/gfx/geometry/rect.h"
#import "ui/gfx/image/image.h"
#import "url/third_party/mozilla/url_parse.h"
#import "url/url_util.h"

using bookmarks::BookmarkModel;
using metrics::OmniboxEventProto;
using omnibox::mojom::NavigationPredictor;

// Helpers --------------------------------------------------------------------

namespace {

// The possible histogram values emitted when escape is pressed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OmniboxEscapeAction {
  // `kNone` doesn't mean escape did nothing (e.g. it could have stopped a
  // navigation), just that it did not affect the omnibox state.
  //  kNone = 0, No longer used since escape now always blurs the omnibox if it
  //             does nothing else.
  kRevertTemporaryText = 1,
  kClosePopup = 2,
  kClearUserInput = 3,
  kClosePopupAndClearUserInput = 4,
  kBlur = 5,
  kMaxValue = kBlur,
};

const char kOmniboxFocusResultedInNavigation[] =
    "Omnibox.FocusResultedInNavigation";

// `executed_selection` indicates which OmniboxAction within `result`
// was executed, and leaving this parameter as the default indicates
// that no action was executed.
void RecordActionShownForAllActions(
    const AutocompleteResult& result,
    OmniboxPopupSelection executed_selection =
        OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch)) {
  // Record the presence of all actions in the result set.
  for (size_t line_index = 0; line_index < result.size(); ++line_index) {
    const AutocompleteMatch& match = result.match_at(line_index);
    // Record the presence of the takeover action on this line, if any.
    if (match.takeover_action) {
      match.takeover_action->RecordActionShown(
          line_index,
          /*executed=*/line_index == executed_selection.line &&
              executed_selection.state == OmniboxPopupSelection::NORMAL);
    }
    for (size_t action_index = 0; action_index < match.actions.size();
         ++action_index) {
      match.actions[action_index]->RecordActionShown(line_index,
                                                     /*executed=*/false);
    }
  }
}

// Find the number of IPv4 parts if the user inputs a URL with an IP address
// host. Returns 0 if the user does not manually types the full IP address.
size_t CountNumberOfIPv4Parts(const std::u16string& text,
                              const GURL& url,
                              size_t completed_length) {
  if (!url.HostIsIPAddress() || !url.SchemeIsHTTPOrHTTPS() ||
      completed_length > 0) {
    return 0;
  }

  url::Parsed parsed = url::ParseStandardURL(text);
  if (!parsed.host.is_valid()) {
    return 0;
  }

  size_t parts = 1;
  bool potential_part = false;
  for (int i = parsed.host.begin; i < parsed.host.end(); i++) {
    if (text[i] == '.') {
      potential_part = true;
    }
    if (potential_part && text[i] >= '0' && text[i] <= '9') {
      parts++;
      potential_part = false;
    }
  }
  return parts;
}

// This function provides a logging implementation that aligns with the original
// definition of the `DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES()` macro, which is
// currently being used to log the `FocusToOpenTimeAnyPopupState3` Omnibox
// metric.
void LogHistogramMediumTimes(const std::string& histogram_name,
                             base::TimeDelta elapsed) {
  base::UmaHistogramCustomTimes(histogram_name, elapsed, base::Milliseconds(10),
                                base::Minutes(3), 50);
}

void LogFocusToOpenTime(base::TimeDelta elapsed,
                        bool is_zero_prefix,
                        PageClassification page_classification,
                        AutocompleteMatch& match,
                        size_t action_index) {
  LogHistogramMediumTimes("Omnibox.FocusToOpenTimeAnyPopupState3", elapsed);

  std::string summarized_result_type;
  switch (OmniboxMetricsProvider::GetClientSummarizedResultType(
      match.GetOmniboxEventResultType(action_index))) {
    case ClientSummarizedResultType::kSearch:
      summarized_result_type = "SEARCH";
      break;
    case ClientSummarizedResultType::kUrl:
      summarized_result_type = "URL";
      break;
    default:
      summarized_result_type = "OTHER";
      break;
  }

  LogHistogramMediumTimes(
      base::StrCat(
          {"Omnibox.FocusToOpenTimeAnyPopupState3.BySummarizedResultType.",
           summarized_result_type}),
      elapsed);

  const std::string page_context =
      OmniboxEventProto::PageClassification_Name(page_classification);
  LogHistogramMediumTimes(
      base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.ByPageContext.",
                    page_context}),
      elapsed);

  LogHistogramMediumTimes(
      base::StrCat(
          {"Omnibox.FocusToOpenTimeAnyPopupState3.BySummarizedResultType.",
           summarized_result_type, ".ByPageContext.", page_context}),
      elapsed);

  if (is_zero_prefix) {
    LogHistogramMediumTimes("Omnibox.FocusToOpenTimeAnyPopupState3.ZeroSuggest",
                            elapsed);
    LogHistogramMediumTimes(
        base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.ZeroSuggest."
                      "BySummarizedResultType.",
                      summarized_result_type}),
        elapsed);
    LogHistogramMediumTimes(
        base::StrCat(
            {"Omnibox.FocusToOpenTimeAnyPopupState3.ZeroSuggest.ByPageContext.",
             page_context}),
        elapsed);
    LogHistogramMediumTimes(
        base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.ZeroSuggest."
                      "BySummarizedResultType.",
                      summarized_result_type, ".ByPageContext.", page_context}),
        elapsed);
  } else {
    LogHistogramMediumTimes(
        "Omnibox.FocusToOpenTimeAnyPopupState3.TypedSuggest", elapsed);
    LogHistogramMediumTimes(
        base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.TypedSuggest."
                      "BySummarizedResultType.",
                      summarized_result_type}),
        elapsed);
    LogHistogramMediumTimes(
        base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.TypedSuggest."
                      "ByPageContext.",
                      page_context}),
        elapsed);
    LogHistogramMediumTimes(
        base::StrCat({"Omnibox.FocusToOpenTimeAnyPopupState3.TypedSuggest."
                      "BySummarizedResultType.",
                      summarized_result_type, ".ByPageContext.", page_context}),
        elapsed);
  }
}

}  // namespace

// OmniboxEditModelIOS
// -----------------------------------------------------------

OmniboxEditModelIOS::OmniboxEditModelIOS(OmniboxControllerIOS* controller,
                                         OmniboxViewBase* view)
    : controller_(controller),
      view_(view),
      user_input_in_progress_(false),
      focus_resulted_in_navigation_(false),
      just_deleted_text_(false),
      paste_state_(NONE),
      control_key_state_(UP),
      in_revert_(false),
      close_lens_(false) {}

OmniboxEditModelIOS::~OmniboxEditModelIOS() = default;

void OmniboxEditModelIOS::set_popup_view(OmniboxPopupViewBase* popup_view) {
  popup_view_ = popup_view;

  // Clear/reset popup-related state.
  old_focused_url_ = GURL();
  popup_selection_ = OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                                           OmniboxPopupSelection::NORMAL);
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
        input_, match, provider_client);
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
         (!has_focus() || (!user_input_in_progress_ && !PopupIsOpen()));
}

std::u16string OmniboxEditModelIOS::GetPermanentDisplayText() const {
  return url_for_editing_;
}

void OmniboxEditModelIOS::SetUserText(const std::u16string& text) {
  SetInputInProgress(true);
  InternalSetUserText(text);
  GetInfoForCurrentText(&current_match_, nullptr);
  paste_state_ = NONE;
}

void OmniboxEditModelIOS::OnChanged() {
  // Don't call CurrentMatch() when there's no editing, as in this case we'll
  // never actually use it.  This avoids running the autocomplete providers (and
  // any systems they then spin up) during startup.
  const AutocompleteMatch& current_match =
      user_input_in_progress_ ? CurrentMatch(nullptr) : AutocompleteMatch();

  controller_->client()->OnTextChanged(
      current_match, user_input_in_progress_, user_text_,
      autocomplete_controller()->result(), has_focus());
}

void OmniboxEditModelIOS::GetDataForURLExport(GURL* url,
                                              std::u16string* title,
                                              gfx::Image* favicon) {
  *url = CurrentMatch(nullptr).destination_url;
  if (*url == controller_->client()->GetURL()) {
    *title = controller_->client()->GetTitle();
    *favicon = controller_->client()->GetFavicon();
  }
}

bool OmniboxEditModelIOS::CurrentTextIsURL() const {
  // If !user_input_in_progress_, we can determine if the text is a URL without
  // starting the autocomplete system. This speeds browser startup.
  return !user_input_in_progress_ ||
         !AutocompleteMatch::IsSearchType(CurrentMatch(nullptr).type);
}

void OmniboxEditModelIOS::AdjustTextForCopy(int sel_min,
                                            std::u16string* text,
                                            GURL* url_from_text,
                                            bool* write_url) {
  DCHECK(text);
  DCHECK(url_from_text);
  DCHECK(write_url);

  *write_url = false;

  // Do not adjust if selection did not start at the beginning of the field.
  if (sel_min != 0) {
    return;
  }

  // If the user has not modified the display text and is copying the whole URL
  // text, copy the omnibox contents as a hyperlink to the current page.
  if (!user_input_in_progress_ && *text == url_for_editing_) {
    *url_from_text = controller_->client()->GetNavigationEntryURL();
    *write_url = true;

    // Don't let users copy Reader Mode page URLs.
    // We display the original article's URL in the omnibox, so users will
    // expect that to be what is copied to the clipboard.
    if (dom_distiller::url_utils::IsDistilledPage(*url_from_text)) {
      *url_from_text = dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(
          *url_from_text);
    }
    *text = base::UTF8ToUTF16(url_from_text->spec());

    return;
  }

  // This code early exits if the copied text looks like a search query. It's
  // not at the very top of this method, as it would interpret the intranet URL
  // "printer/path" as a search query instead of a URL.
  //
  // We can't use CurrentTextIsURL() or GetDataForURLExport() because right now
  // the user is probably holding down control to cause the copy, which will
  // screw up our calculation of the desired_tld.
  AutocompleteMatch match_from_text;
  controller_->client()->GetAutocompleteClassifier()->Classify(
      *text, /*is_keyword_selected=*/false, true, GetPageClassification(),
      &match_from_text, nullptr);
  if (AutocompleteMatch::IsSearchType(match_from_text.type)) {
    return;
  }

  // Make our best GURL interpretation of `text`.
  *url_from_text = match_from_text.destination_url;

  // Get the current page GURL (or the GURL of the currently selected match).
  GURL current_page_url = controller_->client()->GetNavigationEntryURL();
  if (PopupIsOpen()) {
    AutocompleteMatch current_match = CurrentMatch(nullptr);
    if (!AutocompleteMatch::IsSearchType(current_match.type) &&
        current_match.destination_url.is_valid()) {
      // If the popup is open and a valid match is selected, treat that as the
      // current page, since the URL in the Omnibox will be from that match.
      current_page_url = current_match.destination_url;
    }
  }

  // If the user has altered the host piece of the omnibox text, then we cannot
  // guess at user intent, so early exit and leave `text` as-is as plain text.
  if (!current_page_url.SchemeIsHTTPOrHTTPS() ||
      !url_from_text->SchemeIsHTTPOrHTTPS() ||
      current_page_url.host_piece() != url_from_text->host_piece()) {
    return;
  }

  // Infer the correct scheme for the copied text, and prepend it if necessary.
  {
    const std::u16string http =
        base::StrCat({url::kHttpScheme16, url::kStandardSchemeSeparator16});
    const std::u16string https =
        base::StrCat({url::kHttpsScheme16, url::kStandardSchemeSeparator16});

    const std::u16string& current_page_url_prefix =
        current_page_url.SchemeIs(url::kHttpScheme) ? http : https;

    // Only prepend a scheme if the text doesn't already have a scheme.
    if (!base::StartsWith(*text, http, base::CompareCase::INSENSITIVE_ASCII) &&
        !base::StartsWith(*text, https, base::CompareCase::INSENSITIVE_ASCII)) {
      *text = current_page_url_prefix + *text;

      // Amend the copied URL to match the prefixed string.
      GURL::Replacements replace_scheme;
      replace_scheme.SetSchemeStr(current_page_url.scheme_piece());
      *url_from_text = url_from_text->ReplaceComponents(replace_scheme);
    }
  }

  // If the URL derived from `text` is valid, mark `write_url` true, and modify
  // `text` to contain the canonical URL spec with non-ASCII characters escaped.
  if (url_from_text->is_valid()) {
    *write_url = true;
    *text = base::UTF8ToUTF16(url_from_text->spec());
  }
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

  if (changed_to_user_input_in_progress && user_text_.empty()) {
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
  input_.Clear();
  paste_state_ = NONE;
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
  const std::u16string input_text = user_text_;

  size_t start, cursor_position;
  // This method currently only works when there's a view, but ideally the
  // model should be primary for determining such state.
  CHECK(view_);
  view_->GetSelectionBounds(&start, &cursor_position);

  input_ = AutocompleteInput(
      input_text, cursor_position, GetPageClassification(),
      controller_->client()->GetSchemeClassifier(),
      controller_->client()->ShouldDefaultTypedNavigationsToHttps(),
      controller_->client()->GetHttpsPortForTesting(),
      controller_->client()->IsUsingFakeHttpsForHttpsUpgradeTesting());
  input_.set_current_url(controller_->client()->GetURL());
  input_.set_current_title(controller_->client()->GetTitle());
  input_.set_prevent_inline_autocomplete(
      prevent_inline_autocomplete || just_deleted_text_ ||
      (has_selected_text && inline_autocompletion_.empty()) ||
      paste_state_ != NONE);
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
          controller_->client()->GetLensOverlaySuggestInputs()) {
    input_.set_lens_overlay_suggest_inputs(*suggest_inputs);
  }

  controller_->StartAutocomplete(input_);
}

bool OmniboxEditModelIOS::CanPasteAndGo(const std::u16string& text) const {
  if (!controller_->client()->IsPasteAndGoEnabled()) {
    return false;
  }

  AutocompleteMatch match;
  ClassifyString(text, &match, nullptr);
  return match.destination_url.is_valid();
}

void OmniboxEditModelIOS::PasteAndGo(
    const std::u16string& text,
    base::TimeTicks match_selection_timestamp) {
  DCHECK(CanPasteAndGo(text));

  if (view_) {
    view_->RevertAll();
  }
  AutocompleteMatch match;
  GURL alternate_nav_url;
  ClassifyString(text, &match, &alternate_nav_url);

  GURL upgraded_url;
  if (match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED &&
      controller_->client()->ShouldDefaultTypedNavigationsToHttps() &&
      AutocompleteInput::ShouldUpgradeToHttps(
          text, match.destination_url,
          controller_->client()->GetHttpsPortForTesting(),
          controller_->client()->IsUsingFakeHttpsForHttpsUpgradeTesting(),
          &upgraded_url)) {
    input_.set_added_default_scheme_to_typed_url(true);
    DCHECK(upgraded_url.is_valid());
    match.destination_url = upgraded_url;
  } else {
    input_.set_added_default_scheme_to_typed_url(false);
  }

  OpenMatch(OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch), match,
            WindowOpenDisposition::CURRENT_TAB, alternate_nav_url, text,
            match_selection_timestamp);
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
      input_, match, autocomplete_controller()->autocomplete_provider_client());
  OpenMatch(selection, match, disposition, alternate_nav_url, std::u16string(),
            timestamp);
}

void OmniboxEditModelIOS::OpenSelection(base::TimeTicks timestamp,
                                        WindowOpenDisposition disposition) {
  OpenSelection(popup_selection_, timestamp, disposition);
}

void OmniboxEditModelIOS::ClearAdditionalText() {
  TRACE_EVENT0("omnibox", "OmniboxEditModelIOS::ClearAdditionalText");
  if (view_) {
    view_->SetAdditionalText(std::u16string());
  }
}

void OmniboxEditModelIOS::OnSetFocus(bool control_down) {
  TRACE_EVENT0("omnibox", "OmniboxEditModelIOS::OnSetFocus");
  last_omnibox_focus_ = base::TimeTicks::Now();
  focus_resulted_in_navigation_ = false;

  // If the omnibox lost focus while the caret was hidden and then regained
  // focus, OnSetFocus() is called and should restore visibility. Note that
  // focus can be regained without an accompanying call to
  // OmniboxViewBase::SetFocus(), e.g. by tabbing in.
  SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  // On focusing the omnibox, if the ctrl key is pressed, we don't want to
  // trigger ctrl-enter behavior unless it is released and re-pressed. For
  // example, if the user presses ctrl-l to focus the omnibox.
  control_key_state_ = control_down ? DOWN_AND_CONSUMED : UP;

  if (user_input_in_progress_ || !in_revert_) {
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
  if (user_input_in_progress_ && !user_clobbered_permanent_text) {
    return;
  }

  TRACE_EVENT0("omnibox", "OmniboxEditModelIOS::StartZeroSuggestRequest");

  // Send the textfield contents exactly as-is, as otherwise the verbatim
  // match can be wrong. The full page URL is anyways in set_current_url().
  // Don't attempt to use https as the default scheme for these requests.
  input_ = AutocompleteInput(
      GetText(), GetPageClassification(),
      controller_->client()->GetSchemeClassifier(),
      /*should_use_https_as_default_scheme=*/false,
      controller_->client()->GetHttpsPortForTesting(),
      controller_->client()->IsUsingFakeHttpsForHttpsUpgradeTesting());
  input_.set_current_url(controller_->client()->GetURL());
  input_.set_current_title(controller_->client()->GetTitle());
  input_.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  // Set the lens overlay suggest inputs, if available.
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggest_inputs =
          controller_->client()->GetLensOverlaySuggestInputs()) {
    input_.set_lens_overlay_suggest_inputs(*suggest_inputs);
  }
  controller_->StartAutocomplete(input_);
}

void OmniboxEditModelIOS::SetCaretVisibility(bool visible) {
  // Caret visibility only matters if the omnibox has focus.
  if (focus_state_ != OMNIBOX_FOCUS_NONE) {
    SetFocusState(visible ? OMNIBOX_FOCUS_VISIBLE : OMNIBOX_FOCUS_INVISIBLE,
                  OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  }
}

void OmniboxEditModelIOS::ConsumeCtrlKey() {
  if (control_key_state_ == DOWN) {
    control_key_state_ = DOWN_AND_CONSUMED;
  }
}

void OmniboxEditModelIOS::OnWillKillFocus() {
  if (user_input_in_progress_ || !in_revert_) {
    controller_->client()->OnInputStateChanged();
  }
}

void OmniboxEditModelIOS::OnKillFocus() {
  UMA_HISTOGRAM_BOOLEAN(kOmniboxFocusResultedInNavigation,
                        focus_resulted_in_navigation_);
  SetFocusState(OMNIBOX_FOCUS_NONE, OMNIBOX_FOCUS_CHANGE_EXPLICIT);
  last_omnibox_focus_ = base::TimeTicks();
  paste_state_ = NONE;
  control_key_state_ = UP;
#if BUILDFLAG(IS_WIN)
  if (view_) {
    view_->HideImeIfNeeded();
  }
#endif
}

bool OmniboxEditModelIOS::OnEscapeKeyPressed() {
  const char* kOmniboxEscapeHistogramName = "Omnibox.Escape";

  // We do not clear the pending entry from the omnibox when a load is first
  // stopped.  If the user presses Escape while stopped, whether editing or not,
  // we clear it.
  if (controller_->client()->CurrentPageExists() &&
      !controller_->client()->IsLoading()) {
    controller_->client()->DiscardNonCommittedNavigations();
    if (view_) {
      view_->Update();
    }
  }

  // Close the popup if it's open.
  if (PopupIsOpen()) {
    base::UmaHistogramEnumeration(kOmniboxEscapeHistogramName,
                                  OmniboxEscapeAction::kClosePopup);
    if (view_) {
      view_->CloseOmniboxPopup();
    }
    return true;
  }

  // Unconditionally revert/select all.  This ensures any popup, whether due to
  // normal editing or ZeroSuggest, is closed, and the full text is selected.
  // This in turn allows the user to use escape to quickly select all the text
  // for ease of replacement, and matches other browsers.
  bool user_input_was_in_progress = user_input_in_progress_;
  // TODO(crbug.com/40230336): If the popup was open, `user_input_in_progress_`
  //  *should* also be true; checking `user_text_` in the DCHECK below, and
  //  checking `popup_was_open` in the if predicate below *should* be
  //  unnecessary. However, that's not always the case (see
  //  `user_input_in_progress_` comment in the header).
  if (view_) {
    view_->RevertAll();
    view_->SelectAll(true);
  }
  if (user_input_was_in_progress) {
    base::UmaHistogramEnumeration(kOmniboxEscapeHistogramName,
                                  OmniboxEscapeAction::kClearUserInput);
    // If the user was in the midst of editing, don't cancel any underlying page
    // load.  This doesn't match IE or Firefox, but seems more correct.  Note
    // that we do allow the page load to be stopped in the case where
    // ZeroSuggest was visible; this is so that it's still possible to focus the
    // address bar and hit escape once to stop a load even if the address being
    // loaded triggers the ZeroSuggest popup.
    return true;
  }

  // Blur the omnibox and focus the web contents.
  base::UmaHistogramEnumeration(kOmniboxEscapeHistogramName,
                                OmniboxEscapeAction::kBlur);
  controller_->client()->FocusWebContents();
  return true;
}

void OmniboxEditModelIOS::OnControlKeyChanged(bool pressed) {
  if (pressed == (control_key_state_ == UP)) {
    control_key_state_ = pressed ? DOWN : UP;
  }
}

void OmniboxEditModelIOS::OnPaste() {
  UMA_HISTOGRAM_COUNTS_1M("Omnibox.Paste", 1);
  paste_state_ = PASTING;
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

  inline_autocompletion_ = inline_autocompletion;
  if (inline_autocompletion_.empty() && view_) {
    view_->OnInlineAutocompleteTextCleared();
  }

  const std::u16string& user_text =
      user_input_in_progress_ ? user_text_ : input_.text();

    if (view_) {
      view_->OnInlineAutocompleteTextMaybeChanged(user_text,
                                                  inline_autocompletion_);
      view_->SetAdditionalText(additional_text);
    }
  // We need to invoke OnChanged in case the destination url changed (as could
  // happen when control is toggled).
  OnChanged();
}

bool OmniboxEditModelIOS::OnAfterPossibleChange(
    const OmniboxViewBase::StateChanges& state_changes) {
  // Update the paste state as appropriate: if we're just finishing a paste
  // that replaced all the text, preserve that information; otherwise, if we've
  // made some other edit, clear paste tracking.
  if (paste_state_ == PASTING) {
    paste_state_ = PASTED;

    GURL url = GURL(*(state_changes.new_text));
    if (url.is_valid()) {
      controller_->client()->OnUserPastedInOmniboxResultingInValidURL();
    }
  } else if (state_changes.text_differs) {
    paste_state_ = NONE;
  }

  if (state_changes.text_differs || state_changes.selection_differs) {
    // Restore caret visibility whenever the user changes text or selection in
    // the omnibox.
    SetFocusState(OMNIBOX_FOCUS_VISIBLE, OMNIBOX_FOCUS_CHANGE_TYPING);
  }

  // When the user performs an action with the ctrl key pressed down, we assume
  // the ctrl key was intended for that action. If they then press enter without
  // releasing the ctrl key, we prevent "ctrl-enter" behavior.
  ConsumeCtrlKey();

  // If the user text does not need to be changed, return now, so we don't
  // change any other state, lest arrowing around the omnibox do something like
  // reset `just_deleted_text_`.  Note that modifying the selection accepts any
  // inline autocompletion, which results in a user text change.
  if (!state_changes.text_differs &&
      (!state_changes.selection_differs || inline_autocompletion_.empty())) {
    return false;
  }

  InternalSetUserText(*state_changes.new_text);
  just_deleted_text_ = state_changes.just_deleted_text;

  if (view_) {
    view_->UpdatePopup();
  }

  return true;
}

// Merge OnPopupDataChanged with this method once the popup
// handling has completely migrated to omnibox_controller.
void OmniboxEditModelIOS::OnCurrentMatchChanged() {

  DCHECK(autocomplete_controller()->result().default_match());
  const AutocompleteMatch& match =
      *autocomplete_controller()->result().default_match();

  OnPopupResultChanged();

  // OnPopupDataChanged() resets OmniboxControllerIOS's `current_match_` early
  // on.  Therefore, copy match.inline_autocompletion to a temp to preserve
  // its value across the entire call.
  OnPopupDataChanged(match.inline_autocompletion,

                     match.additional_text, match);
}

// static
const char OmniboxEditModelIOS::kCutOrCopyAllTextHistogram[] =
    "Omnibox.CutOrCopyAllText";

void OmniboxEditModelIOS::SetAccessibilityLabel(
    const AutocompleteMatch& match) {
  if (view_) {
    view_->SetAccessibilityLabel(view_->GetText(), match, true);
  }
}

void OmniboxEditModelIOS::InternalSetUserText(const std::u16string& text) {
  user_text_ = text;
  just_deleted_text_ = false;
  inline_autocompletion_.clear();
  if (view_) {
    view_->OnInlineAutocompleteTextCleared();
  }
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
    } else if (PopupIsOpen() &&
               GetPopupSelection().line != OmniboxPopupSelection::kNoMatch) {
      const OmniboxPopupSelection selection = GetPopupSelection();
      const AutocompleteMatch& selected_match =
          autocomplete_controller()->result().match_at(selection.line);
      *match = selected_match;
      found_match_for_text = true;
    }
    if (found_match_for_text && alternate_nav_url) {
      AutocompleteProviderClient* provider_client =
          autocomplete_controller()->autocomplete_provider_client();
      *alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
          input_, *match, provider_client);
    }
  }

  if (!found_match_for_text) {
    // For match generation, we use the unelided `url_for_editing_`, unless the
    // user input is in progress.
    std::u16string text_for_match_generation =
        user_input_in_progress() ? user_text_ : url_for_editing_;

    controller_->client()->GetAutocompleteClassifier()->Classify(
        text_for_match_generation, false, true, GetPageClassification(), match,
        alternate_nav_url);
  }
}

bool OmniboxEditModelIOS::IsStarredMatch(const AutocompleteMatch& match) const {
  auto* bookmark_model = controller_->client()->GetBookmarkModel();
  return bookmark_model && bookmark_model->IsBookmarked(match.destination_url);
}

std::u16string OmniboxEditModelIOS::GetSuggestionGroupHeaderText(
    const std::optional<omnibox::GroupId>& suggestion_group_id) const {
  bool force_hide_row_header =
      OmniboxFieldTrial::IsHideSuggestionGroupHeadersEnabledInContext(
          autocomplete_controller()->input().current_page_classification());

  return suggestion_group_id.has_value() && !force_hide_row_header
             ? autocomplete_controller()->result().GetHeaderForSuggestionGroup(
                   suggestion_group_id.value())
             : u"";
}

bool OmniboxEditModelIOS::PopupIsOpen() const {
  return popup_view_ && popup_view_->IsOpen();
}

OmniboxPopupSelection OmniboxEditModelIOS::GetPopupSelection() const {
  DCHECK(popup_view_);
  return popup_selection_;
}

void OmniboxEditModelIOS::OnPopupResultChanged() {
  if (!popup_view_) {
    return;
  }
  const AutocompleteResult& result = autocomplete_controller()->result();

  if (result.default_match()) {
    OmniboxPopupSelection selection = GetPopupSelection();
    selection.line = 0;
    selection.state = OmniboxPopupSelection::NORMAL;
    popup_selection_ = selection;
  } else {
    popup_selection_ = OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                                             OmniboxPopupSelection::NORMAL);
  }
  popup_view_->UpdatePopupAppearance();
}

void OmniboxEditModelIOS::SetAutocompleteInput(AutocompleteInput input) {
  input_ = std::move(input);
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

  // If CTRL is down it means the user wants to append ".com" to the text they
  // typed. If we can successfully generate a URL_WHAT_YOU_TYPED match doing
  // that, then we use this. These matches are marked as generated by the
  // HistoryURLProvider so we only generate them if this provider is present.
  bool accept_via_control_enter =
      control_key_state_ == DOWN &&
      autocomplete_controller()->history_url_provider();
  base::UmaHistogramBoolean("Omnibox.Search.CtrlEnter.Used",
                            accept_via_control_enter);
  if (accept_via_control_enter) {
    // For generating the hostname of the URL, we use the most recent
    // input instead of the currently visible text. This means we'll ignore any
    // visible inline autocompletion: if a user types "foo" and is autocompleted
    // to "foodnetwork.com", ctrl-enter will navigate to "foo.com", not
    // "foodnetwork.com".  At the time of writing, this behavior matches
    // Internet Explorer, but not Firefox. Two exceptions to our own rule:
    //  1. If the user has selected a suggestion, use the suggestion text.
    //  2. If the user has never edited the text, use the current page's full
    //     URL instead of the elided URL to avoid HTTPS downgrading.
    std::u16string text_for_desired_tld_navigation = input_.text();
    if (!user_input_in_progress()) {
      text_for_desired_tld_navigation = url_for_editing_;
    }

    // Generate a new AutocompleteInput, copying the latest one but using "com"
    // as the desired TLD. Then use this autocomplete input to generate a
    // URL_WHAT_YOU_TYPED AutocompleteMatch.
    AutocompleteInput input(
        text_for_desired_tld_navigation, input_.cursor_position(), "com",
        input_.current_page_classification(),
        controller_->client()->GetSchemeClassifier(),
        controller_->client()->ShouldDefaultTypedNavigationsToHttps(), 0,
        false);
    input.set_prevent_inline_autocomplete(input_.prevent_inline_autocomplete());
    input.set_omit_asynchronous_matches(input_.omit_asynchronous_matches());
    input.set_focus_type(input_.focus_type());
    input_ = input;
    AutocompleteMatch url_match(VerbatimMatchForInput(
        autocomplete_controller()->history_url_provider(),
        autocomplete_controller()->autocomplete_provider_client(), input_,
        input_.canonicalized_url(), false));

    base::UmaHistogramBoolean("Omnibox.Search.CtrlEnter.ResolvedAsUrl",
                              url_match.destination_url.is_valid());

    if (url_match.destination_url.is_valid()) {
      // We have a valid URL, we use this newly generated AutocompleteMatch.
      match = url_match;
      alternate_nav_url = GURL();
    }
  }

  if (!match.destination_url.is_valid()) {
    return;
  }

  if (paste_state_ != NONE &&
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
      now - time_user_first_modified_omnibox_);
  autocomplete_controller()
      ->UpdateMatchDestinationURLWithAdditionalSearchboxStats(
          elapsed_time_since_user_first_modified_omnibox, &match);

  GURL destination_url = action ? action->getUrl() : match.destination_url;

  // Save the result of the interaction, but do not record the histogram yet.
  focus_resulted_in_navigation_ = true;

  RecordActionShownForAllActions(autocomplete_controller()->result(),
                                 selection);
  HistoryFuzzyProvider::RecordOpenMatchMetrics(
      autocomplete_controller()->result(), match);

  std::u16string input_text(pasted_text);
  if (input_text.empty()) {
    input_text = user_input_in_progress_ ? user_text_ : url_for_editing_;
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
  if (input_.IsZeroSuggest() || !pasted_text.empty()) {
    elapsed_time_since_user_first_modified_omnibox = default_time_delta;
    elapsed_time_since_last_change_to_default_match = default_time_delta;
  }

  base::TimeDelta elapsed_time_since_user_focused_omnibox = default_time_delta;
  if (!last_omnibox_focus_.is_null()) {
    elapsed_time_since_user_focused_omnibox = now - last_omnibox_focus_;
    // Only record focus to open time when a focus actually happened (as
    // opposed to, say, dragging a link onto the omnibox).
    LogFocusToOpenTime(elapsed_time_since_user_focused_omnibox,
                       input_.IsZeroSuggest(), GetPageClassification(), match,
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
      input_.IsZeroSuggest() ? std::u16string() : input_text;
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
      user_text, just_deleted_text_, input_.type(),
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
  size_t ipv4_parts_count =
      CountNumberOfIPv4Parts(user_text, destination_url, completed_length);
  // The histogram is collected to decide if shortened IPv4 addresses
  // like 127.1 should be deprecated.
  // Only valid IP addresses manually inputted by the user will be counted.
  if (ipv4_parts_count > 0) {
    base::UmaHistogramCounts100("Omnibox.IPv4AddressPartsCount",
                                ipv4_parts_count);
  }

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
    base::AutoReset<bool> tmp(&in_revert_, true);
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
      base::AutoReset<bool> tmp(&in_revert_, true);

      controller_->client()->OnAutocompleteAccept(
          destination_url, match.post_content.get(), disposition,
          ui::PageTransitionFromInt(match.transition |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          match.type, match_selection_timestamp,
          input_.added_default_scheme_to_typed_url(),
          input_.typed_url_had_http_scheme() &&
              match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED,
          input_text, match,
          VerbatimMatchForInput(
              autocomplete_controller()->history_url_provider(),
              autocomplete_controller()->autocomplete_provider_client(),
              alternate_input, alternate_nav_url, false));
    }
  }
}

void OmniboxEditModelIOS::ClassifyString(const std::u16string& text,
                                         AutocompleteMatch* match,
                                         GURL* alternate_nav_url) const {
  DCHECK(match);
  controller_->client()->GetAutocompleteClassifier()->Classify(
      text, false, false, GetPageClassification(), match, alternate_nav_url);
}

bool OmniboxEditModelIOS::SetInputInProgressNoNotify(bool in_progress) {
  if (user_input_in_progress_ == in_progress) {
    return false;
  }

  user_input_in_progress_ = in_progress;
  if (user_input_in_progress_) {
    time_user_first_modified_omnibox_ = base::TimeTicks::Now();
    base::RecordAction(base::UserMetricsAction("OmniboxInputInProgress"));
    autocomplete_controller()->ResetSession();
  }
  return true;
}

void OmniboxEditModelIOS::NotifyObserversInputInProgress(bool in_progress) {
  controller_->client()->OnInputInProgress(in_progress);

  if (user_input_in_progress_ || !in_revert_) {
    controller_->client()->OnInputStateChanged();
  }
}

void OmniboxEditModelIOS::SetFocusState(OmniboxFocusState state,
                                        OmniboxFocusChangeReason reason) {
  if (state == focus_state_) {
    return;
  }

  // Update state and notify view if the omnibox has focus and the caret
  // visibility changed.
  const bool was_caret_visible = is_caret_visible();
  focus_state_ = state;
  if (focus_state_ != OMNIBOX_FOCUS_NONE &&
      is_caret_visible() != was_caret_visible && view_) {
    view_->ApplyCaretVisibility();
  }

  controller_->client()->OnFocusChanged(focus_state_, reason);
}

std::u16string OmniboxEditModelIOS::GetText() const {
  // Once the model owns primary text, the check for `view_` won't be needed.
  if (view_) {
    return view_->GetText();
  } else {
    NOTREACHED();
  }
}
