// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_metrics_recorder.h"

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/time/time.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/navigation_metrics/navigation_metrics.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_type.h"
#import "components/omnibox/browser/autocomplete_provider.h"
#import "components/omnibox/browser/autocomplete_provider_client.h"
#import "components/omnibox/browser/history_fuzzy_provider.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_event_global_tracker.h"
#import "components/omnibox/browser/omnibox_log.h"
#import "components/omnibox/browser/omnibox_logging_utils.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "net/cookies/cookie_util.h"
#import "third_party/metrics_proto/omnibox_event.pb.h"
#import "third_party/metrics_proto/omnibox_focus_type.pb.h"

using bookmarks::BookmarkModel;
using metrics::OmniboxEventProto;

namespace {

/// Default value for time logging in the omnibox. This value will be ignored
/// during analysis.
constexpr base::TimeDelta kDefaultTimeDelta = base::Milliseconds(-1);

}  // namespace

@implementation OmniboxMetricsRecorder {
  /// The omnibox client.
  raw_ptr<OmniboxClient, DanglingUntriaged> _omniboxClient;
  /// The omnibox text model used to retrieve the text state.
  raw_ptr<const OmniboxTextModel, DanglingUntriaged> _omniboxTextModel;
  /// The autocomplete controller.
  raw_ptr<const AutocompleteController, DanglingUntriaged>
      _autocompleteController;
  /// The number of lines in the omnibox text view.
  NSInteger _numberOfLines;
}

- (instancetype)initWithClient:(OmniboxClient*)omniboxClient
                     textModel:(const OmniboxTextModel*)omniboxTextModel {
  self = [super init];
  if (self) {
    _omniboxClient = omniboxClient;
    _omniboxTextModel = omniboxTextModel;
  }
  return self;
}

- (void)disconnect {
  _omniboxClient = nullptr;
  _omniboxTextModel = nullptr;
  _autocompleteController = nullptr;
  _omniboxAutocompleteController = nullptr;
}

- (void)setAutocompleteController:
    (AutocompleteController*)autocompleteController {
  _autocompleteController = autocompleteController;
}

- (void)setNumberOfLines:(NSInteger)numberOfLines {
  _numberOfLines = numberOfLines;
}

- (void)recordOpenMatch:(AutocompleteMatch)match
           destinationURL:(GURL)destinationURL
                inputText:(const std::u16string&)inputText
           popupSelection:(OmniboxPopupSelection)selection
    windowOpenDisposition:(WindowOpenDisposition)disposition
                 isAction:(BOOL)isAction
             isPastedText:(BOOL)isPastedText {
  if (_numberOfLines) {
    base::UmaHistogramExactLinear("IOS.Omnibox.NumberOfLines", _numberOfLines,
                                  20);
  }
  const base::TimeTicks& now(base::TimeTicks::Now());
  base::TimeDelta elapsedTimeSinceUserFirstModifiedOmnibox = [self
      elapsedTimeSinceUserFirstModifiedOmniboxWithPastedText:isPastedText];

  omnibox::RecordActionShownForAllActions(_autocompleteController->result(),
                                          selection);
  HistoryFuzzyProvider::RecordOpenMatchMetrics(
      _autocompleteController->result(), match);

  base::TimeDelta elapsedTimeSinceLastChangeToDefaultMatch(
      now - _autocompleteController->last_time_default_match_changed());
  DCHECK(match.provider);
  // These elapsed times don't really make sense for matches that come from
  // omnibox focus (because the user did not modify the omnibox), so for those
  // we set the elapsed times to something that will be ignored by
  // metrics_log.cc.  They also don't necessarily make sense if the omnibox
  // dropdown is closed or the user used paste-and-go.  (In most
  // cases when this happens, the user never modified the omnibox.)
  const bool popupIsOpen = self.omniboxAutocompleteController.hasSuggestions;
  if (_omniboxTextModel->input.IsZeroSuggest() || isPastedText) {
    elapsedTimeSinceLastChangeToDefaultMatch = kDefaultTimeDelta;
  }

  base::TimeDelta elapsedTimeSinceUserFocusedOmnibox = kDefaultTimeDelta;
  if (!_omniboxTextModel->last_omnibox_focus.is_null()) {
    elapsedTimeSinceUserFocusedOmnibox =
        now - _omniboxTextModel->last_omnibox_focus;
    // Only record focus to open time when a focus actually happened (as
    // opposed to, say, dragging a link onto the omnibox).

    omnibox::LogFocusToOpenTime(
        elapsedTimeSinceUserFocusedOmnibox,
        _omniboxTextModel->input.IsZeroSuggest(),
        _omniboxClient->GetPageClassification(/*is_prefetch=*/false), match,
        selection.IsAction() ? selection.action_index : -1);
  }

  // In some unusual cases, we ignore _autocompleteController->result() and
  // instead log a fake result set with a single element (`match`) and
  // selected_index of 0. For these cases:
  //  1. If the popup is closed (there is no result set). This doesn't apply
  //  for WebUI searchboxes since they don't have an associated popup.
  //  2. If the index is out of bounds. This should only happen if
  //  `selection.line` is
  //     kNoMatch, which can happen if the default search provider is disabled.
  //  3. If this is paste-and-go (meaning the contents of the dropdown
  //     are ignored regardless).
  const bool dropdownIgnored =
      !popupIsOpen ||
      selection.line >= _autocompleteController->result().size() ||
      isPastedText;
  ACMatches fakeSingleEntryMatches;
  fakeSingleEntryMatches.push_back(match);
  AutocompleteResult fakeSingleEntryResult;
  fakeSingleEntryResult.AppendMatches(fakeSingleEntryMatches);

  std::u16string userText =
      _omniboxTextModel->input.IsZeroSuggest() ? std::u16string() : inputText;
  size_t completedLength = match.allowed_to_be_default_match
                               ? match.inline_autocompletion.length()
                               : std::u16string::npos;
  bool isIncognito =
      _autocompleteController->autocomplete_provider_client()->IsOffTheRecord();
  OmniboxLog log(
      userText, _omniboxTextModel->just_deleted_text,
      _omniboxTextModel->input.type(),
      /*is_keyword_selected=*/false, OmniboxEventProto::INVALID, popupIsOpen,
      dropdownIgnored ? OmniboxPopupSelection(0) : selection, disposition,
      isPastedText,
      SessionID::InvalidValue(),  // don't know tab ID; set later if appropriate
      _omniboxClient->GetPageClassification(/*is_prefetch=*/false),
      elapsedTimeSinceUserFirstModifiedOmnibox, completedLength,
      elapsedTimeSinceLastChangeToDefaultMatch,
      dropdownIgnored ? fakeSingleEntryResult
                      : _autocompleteController->result(),
      destinationURL, isIncognito, _omniboxTextModel->input.IsZeroSuggest(),
      match.session);
  log.elapsed_time_since_user_focused_omnibox =
      elapsedTimeSinceUserFocusedOmnibox;
  log.ukm_source_id = _omniboxClient->GetUKMSourceId();

  if ((disposition == WindowOpenDisposition::CURRENT_TAB) &&
      _omniboxClient->CurrentPageExists()) {
    // If we know the destination is being opened in the current tab,
    // we can easily get the tab ID.  (If it's being opened in a new
    // tab, we don't know the tab ID yet.)
    log.tab_id = _omniboxClient->GetSessionID();
  }
  _autocompleteController->AddProviderAndTriggeringLogs(&log);

  base::UmaHistogramEnumeration("Omnibox.SuggestionUsed.RichAutocompletion",
                                match.rich_autocompletion_triggered);

  omnibox::LogIPv4PartsCount(userText, destinationURL, completedLength);

  _omniboxClient->OnURLOpenedFromOmnibox(&log);
  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  LOCAL_HISTOGRAM_BOOLEAN("Omnibox.EventCount", true);
  omnibox::answer_data_parser::LogAnswerUsed(match.answer_type);

  TemplateURLService* service = _omniboxClient->GetTemplateURLService();
  TemplateURL* templateURL = match.GetTemplateURL(service);
  if (templateURL) {
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
        !isPastedText) {
      navigation_metrics::RecordOmniboxURLNavigation(destinationURL);
    }

    // The following histograms should be recorded for both TYPED and pasted
    // URLs, but should still exclude reloads.
    if (ui::PageTransitionTypeIncludingQualifiersIs(
            match.transition, ui::PAGE_TRANSITION_TYPED) ||
        ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                    ui::PAGE_TRANSITION_LINK)) {
      net::cookie_util::RecordCookiePortOmniboxHistograms(destinationURL);
    }
  }

  if (!isAction) {
    // Track whether the destination URL sends us to a search results page
    // using the default search provider.
    TemplateURLService* templateURLService =
        _omniboxClient->GetTemplateURLService();
    if (templateURLService &&
        templateURLService->IsSearchResultsPageFromDefaultSearchProvider(
            match.destination_url)) {
      base::RecordAction(
          base::UserMetricsAction("OmniboxDestinationURLIsSearchOnDSP"));
      base::UmaHistogramBoolean("Omnibox.Search.OffTheRecord", isIncognito);
    }

    BookmarkModel* bookmarkModel = _omniboxClient->GetBookmarkModel();
    if (bookmarkModel && bookmarkModel->IsBookmarked(destinationURL)) {
      _omniboxClient->OnBookmarkLaunched();
    }
  }
}

- (base::TimeDelta)elapsedTimeSinceUserFirstModifiedOmniboxWithPastedText:
    (BOOL)isPastedText {
  const base::TimeTicks& now(base::TimeTicks::Now());
  base::TimeDelta elapsedTimeSinceUserFirstModifiedOmnibox(
      now - _omniboxTextModel->time_user_first_modified_omnibox);
  // These elapsed times don't really make sense for matches that come from
  // omnibox focus (because the user did not modify the omnibox), so for those
  // we set the elapsed times to something that will be ignored by
  // metrics_log.cc.  They also don't necessarily make sense if the omnibox
  // dropdown is closed or the user used paste-and-go.  (In most
  // cases when this happens, the user never modified the omnibox.)
  if (_omniboxTextModel->input.IsZeroSuggest() || isPastedText) {
    elapsedTimeSinceUserFirstModifiedOmnibox = kDefaultTimeDelta;
  }
  return elapsedTimeSinceUserFirstModifiedOmnibox;
}

@end
