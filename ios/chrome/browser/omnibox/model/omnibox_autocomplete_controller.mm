// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"

#import <optional>
#import <string>

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/trace_event/trace_event.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_controller_config.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/browser/clipboard_provider.h"
#import "components/omnibox/browser/history_url_provider.h"
#import "components/omnibox/browser/lens_suggest_inputs_utils.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/omnibox/browser/omnibox_pref_names.h"
#import "components/omnibox/browser/page_classification_functions.h"
#import "components/omnibox/browser/verbatim_match.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_controller_observer_bridge.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller_debugger_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_lens_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_metrics_recorder.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_result_wrapper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "third_party/omnibox_proto/groups.pb.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

using base::UserMetricsAction;

@interface OmniboxAutocompleteController () <AutocompleteControllerObserver,
                                             BooleanObserver>

/// Redefined as a readwrite
@property(nonatomic, assign, readwrite) BOOL hasSuggestions;

@end

@implementation OmniboxAutocompleteController {
  /// Client of the omnibox.
  raw_ptr<OmniboxClient, DanglingUntriaged> _omniboxClient;
  /// Omnibox text model.
  raw_ptr<OmniboxTextModel, DanglingUntriaged> _omniboxTextModel;

  /// The autocomplete controller.
  raw_ptr<AutocompleteController> _autocompleteController;
  /// Autocomplete controller observer.
  std::unique_ptr<AutocompleteControllerObserverBridge>
      _autocompleteControllerObserverBridge;
  /// Pref tracking if the bottom omnibox is enabled.
  PrefBackedBoolean* _bottomOmniboxEnabled;
  /// Preferred omnibox position, logged in omnibox logs.
  metrics::OmniboxEventProto::OmniboxPosition _preferredOmniboxPosition;
  /// Where the omnibox is presented from.
  OmniboxPresentationContext _omniboxPresentationContext;
}

- (instancetype)
     initWithOmniboxClient:(OmniboxClient*)omniboxClient
    autocompleteController:(AutocompleteController*)autocompleteController
          omniboxTextModel:(OmniboxTextModel*)omniboxTextModel
       presentationContext:(OmniboxPresentationContext)presentationContext {
  self = [super init];
  if (self) {
    _omniboxClient = omniboxClient;
    _autocompleteController = autocompleteController;
    _omniboxTextModel = omniboxTextModel;
    _omniboxPresentationContext = presentationContext;

    _autocompleteControllerObserverBridge =
        std::make_unique<AutocompleteControllerObserverBridge>(self);
    _autocompleteController->AddObserver(
        _autocompleteControllerObserverBridge.get());
    _preferredOmniboxPosition = metrics::OmniboxEventProto::UNKNOWN_POSITION;
    _bottomOmniboxEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()
                   prefName:omnibox::kIsOmniboxInBottomPosition];
    [_bottomOmniboxEnabled setObserver:self];
    // Initialize to the correct value.
    [self booleanDidChange:_bottomOmniboxEnabled];
  }
  return self;
}

- (void)disconnect {
  if (_autocompleteControllerObserverBridge && _autocompleteController) {
    _autocompleteController->RemoveObserver(
        _autocompleteControllerObserverBridge.get());
    _autocompleteControllerObserverBridge.reset();
  }
  [self.autocompleteResultWrapper disconnect];
  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;
  _autocompleteResultWrapper = nil;
  _autocompleteController = nullptr;
  _omniboxTextModel = nullptr;
  _omniboxClient = nullptr;
}

- (AutocompleteController*)autocompleteController {
  return _autocompleteController;
}

- (void)updatePopupSuggestions {
  if (_autocompleteController) {
    BOOL isFocusing = _autocompleteController->input().focus_type() ==
                      metrics::OmniboxFocusType::INTERACTION_FOCUS;

    self.hasSuggestions = !_autocompleteController->result().empty();
    [self.delegate
        omniboxAutocompleteControllerDidUpdateSuggestions:self
                                           hasSuggestions:self.hasSuggestions
                                               isFocusing:isFocusing];
    [self.debuggerDelegate omniboxAutocompleteController:self
                       didUpdateWithSuggestionsAvailable:self.hasSuggestions];
  }
}

- (void)stopAutocompleteWithClearSuggestions:(BOOL)clearSuggestions {
  if (_autocompleteController) {
    TRACE_EVENT0("omnibox", "OmniboxAutocompleteController::StopAutocomplete");
    _autocompleteController->Stop(clearSuggestions
                                      ? AutocompleteStopReason::kClobbered
                                      : AutocompleteStopReason::kInteraction);
  }
}

- (void)openSelection:(OmniboxPopupSelection)selection
            timestamp:(base::TimeTicks)timestamp
          disposition:(WindowOpenDisposition)disposition {
  if (!_autocompleteController) {
    return;
  }
  // Intentionally accept input when selection has no line.
  // This will usually reach `OpenMatch` indirectly.
  if (selection.line >= _autocompleteController->result().size()) {
    [self acceptInputWithDisposition:disposition timestamp:timestamp];
    return;
  }

  const AutocompleteMatch& match =
      _autocompleteController->result().match_at(selection.line);

  // Open the match.
  GURL alternate_nav_url = AutocompleteResult::ComputeAlternateNavUrl(
      _omniboxTextModel->input, match,
      self.autocompleteController->autocomplete_provider_client());
  [self openMatch:match
               popupSelection:selection
        windowOpenDisposition:disposition
              alternateNavURL:alternate_nav_url
                   pastedText:u""
      matchSelectionTimestamp:timestamp];
}

- (void)openCurrentSelectionWithDisposition:(WindowOpenDisposition)disposition
                                  timestamp:(base::TimeTicks)timestamp {
  [self openSelection:OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch,
                                            OmniboxPopupSelection::NORMAL)
            timestamp:timestamp
          disposition:disposition];
}

#pragma mark - properties

- (AutocompleteProviderClient*)autocompleteProviderClient {
  if (!_autocompleteController) {
    return nullptr;
  }
  return _autocompleteController->autocomplete_provider_client();
}

#pragma mark - AutocompleteControllerObserver

- (void)autocompleteController:(AutocompleteController*)autocompleteController
    didUpdateResultChangingDefaultMatch:(BOOL)defaultMatchChanged {
  TRACE_EVENT0("omnibox", "OmniboxAutocompleteController::OnResultChanged");
  DCHECK(autocompleteController == _autocompleteController);
  DCHECK(_omniboxClient);

  const bool popup_was_open = self.hasSuggestions;

  [self updatePopupSuggestions];
  if (defaultMatchChanged) {
    // The default match has changed, we need to let the text controller
    // know about new inline autocomplete text (blue highlight).
    if (const AutocompleteMatch* match =
            _autocompleteController->result().default_match()) {
      // onPopupDataChanged resets text model's `current_match_` early
      // on.  Therefore, copy match.inline_autocompletion to a temp to preserve
      // its value across the entire call.
      [self.omniboxTextController
          onPopupDataChanged:match->inline_autocompletion
              additionalText:match->additional_text
                    newMatch:*match];
    } else {
      [self.omniboxTextController onPopupDataChanged:std::u16string()
                                      additionalText:std::u16string()
                                            newMatch:AutocompleteMatch()];
    }
  }

  const bool popup_is_open = self.hasSuggestions;
  if (popup_was_open != popup_is_open && _omniboxClient) {
    _omniboxClient->OnPopupVisibilityChanged(popup_is_open);
  }

  if (popup_was_open && !popup_is_open) {
    // Closing the popup can change the default suggestion. This usually occurs
    // when it's unclear whether the input represents a search or URL; e.g.,
    // 'a.com/b c' or when title autocompleting. Clear the additional text to
    // avoid suggesting the omnibox contains a URL suggestion when that may no
    // longer be the case; i.e. when the default suggestion changed from a URL
    // to a search suggestion upon closing the popup.
    TRACE_EVENT0("omnibox",
                 "OmniboxAutocompleteController::ClearAdditionalText");
    [self.omniboxTextController setAdditionalText:std::u16string()];
  }

  // Note: The client outlives `this`, so bind a weak pointer to the callback
  // passed in to eliminate the potential for crashes on shutdown.
  // `should_preload` is set to `controller->done()` as prerender may only want
  // to start preloading a result after all Autocomplete results are ready.
  if (_omniboxClient) {
    _omniboxClient->OnResultChanged(
        autocompleteController->result(), defaultMatchChanged,
        /*should_preload=*/autocompleteController->done(),
        /*on_bitmap_fetched=*/base::DoNothing());
  }
}

#pragma mark - AutocompleteResultWrapperDelegate

- (void)autocompleteResultWrapper:(AutocompleteResultWrapper*)wrapper
              didInvalidatePedals:(NSArray<id<AutocompleteSuggestionGroup>>*)
                                      nonPedalSuggestionsGroups {
  [self.delegate omniboxAutocompleteController:self
                    didUpdateSuggestionsGroups:nonPedalSuggestionsGroups];
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxEnabled) {
    _preferredOmniboxPosition =
        _bottomOmniboxEnabled.value
            ? metrics::OmniboxEventProto::BOTTOM_POSITION
            : metrics::OmniboxEventProto::TOP_POSITION;
    _autocompleteController->SetSteadyStateOmniboxPosition(
        _preferredOmniboxPosition);
  }
}

#pragma mark - OmniboxPopup event

- (void)requestSuggestionsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount {
  if (!_autocompleteController) {
    return;
  }
  size_t resultSize = _autocompleteController->result().size();
  // If no suggestions are visible, consider all of them visible.
  if (visibleSuggestionCount == 0) {
    visibleSuggestionCount = resultSize;
  }
  NSUInteger visibleSuggestions = MIN(visibleSuggestionCount, resultSize);
  if (visibleSuggestions > 0) {
    // Groups visible suggestions by search vs url. Skip the first suggestion
    // because it's the omnibox content.
    _autocompleteController->GroupSuggestionsBySearchVsURL(1,
                                                           visibleSuggestions);
  }
  // Groups hidden suggestions by search vs url.
  if (visibleSuggestions < resultSize) {
    _autocompleteController->GroupSuggestionsBySearchVsURL(visibleSuggestions,
                                                           resultSize);
  }

  [self updateWithSortedResults:_autocompleteController->result()];
}

- (void)selectMatchForOpening:(AutocompleteMatch&)match
     withCustomDestinationURL:(GURL)destinationURL
                        inRow:(NSUInteger)row
                       openIn:(WindowOpenDisposition)disposition {
  const auto matchSelectionTimestamp = base::TimeTicks();
  match.destination_url = destinationURL;

  [self openMatch:match
               popupSelection:OmniboxPopupSelection(OmniboxPopupSelection(row))
        windowOpenDisposition:disposition
              alternateNavURL:GURL()
                   pastedText:u""
      matchSelectionTimestamp:matchSelectionTimestamp];
}

- (void)selectMatchForOpening:(const AutocompleteMatch&)match
                        inRow:(NSUInteger)row
                       openIn:(WindowOpenDisposition)disposition {
  const auto matchSelectionTimestamp = base::TimeTicks();
  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));

  if (match.type == AutocompleteMatchType::CLIPBOARD_URL) {
    base::RecordAction(UserMetricsAction("MobileOmniboxClipboardToURL"));
    base::UmaHistogramLongTimes100(
        "MobileOmnibox.PressedClipboardSuggestionAge",
        ClipboardRecentContent::GetInstance()->GetClipboardContentAge());
  }

  if (!_autocompleteController) {
    return;
  }

  // Sometimes the match provided does not correspond to the autocomplete
  // result match specified by `index`. Most Visited Tiles, for example,
  // provide ad hoc matches that are not in the result at all.
  if (row >= _autocompleteController->result().size() ||
      _autocompleteController->result().match_at(row).destination_url !=
          match.destination_url) {
    [self openCustomMatch:match
               disposition:disposition
        selectionTimestamp:matchSelectionTimestamp];
    return;
  }

  // Clipboard match handling.
  if (match.destination_url.is_empty() &&
      AutocompleteMatch::IsClipboardType(match.type)) {
    [self openClipboardMatch:match
                 disposition:disposition
          selectionTimestamp:matchSelectionTimestamp];
    return;
  }

  [self openSelection:OmniboxPopupSelection(row)
            timestamp:matchSelectionTimestamp
          disposition:disposition];
}

- (void)selectMatchForAppending:(const AutocompleteMatch&)match {
  // Make a defensive copy of `match.fill_into_edit`, as CopyToOmnibox() will
  // trigger a new round of autocomplete and modify `match`.
  std::u16string fill_into_edit(match.fill_into_edit);

  // If the match is not a URL, append a whitespace to the end of it.
  if (AutocompleteMatch::IsSearchType(match.type)) {
    fill_into_edit.append(1, ' ');
  }

  [self.omniboxTextController refineWithText:fill_into_edit];
}

- (void)selectMatchForDeletion:(const AutocompleteMatch&)match {
  if (_autocompleteController) {
    _autocompleteController->DeleteMatch(match);
  }
}

- (void)onScroll {
  [self.omniboxTextController onScroll];
}

- (void)onCallAction {
  [self.omniboxTextController hideKeyboard];
}

#pragma mark - OmniboxText events

// Clears any existing suggestions and restarts the autocomplete process.
// If there is text in the omnibox, it triggers a regular autocomplete request.
// If the omnibox is empty, it triggers a zero-suggest request.
- (void)clearAndRestartAutocomplete {
  if (!_autocompleteController || !_omniboxTextModel) {
    return;
  }

  [self stopAutocompleteWithClearSuggestions:YES];

  // Restart autocomplete with the current text.
  std::u16string text = _omniboxTextModel->user_text;
  size_t cursorPosition = _omniboxTextModel->input.cursor_position();

  if (text.length() != 0) {
    [self startAutocompleteWithText:text
                     cursorPosition:cursorPosition
          preventInlineAutocomplete:NO];
  } else {
    // Force a fresh autocomplete request to ensure updated suggestions are
    // fetched, treating this as an explicit user action.
    [self startZeroSuggestRequestWithText:u"" userClobbered:YES];
  }
}

- (void)startAutocompleteWithText:(const std::u16string&)text
                   cursorPosition:(size_t)cursorPosition
        preventInlineAutocomplete:(bool)preventInlineAutocomplete {
  if (!_omniboxClient || !_omniboxTextModel) {
    return;
  }

  // Use text_model()->input during the refactoring while the edit model is
  // still using it crbug.com/390409559.
  _omniboxTextModel->input = AutocompleteInput(
      text, cursorPosition,
      _omniboxClient->GetPageClassification(/*is_prefetch=*/false),
      _omniboxClient->GetSchemeClassifier(),
      _omniboxClient->ShouldDefaultTypedNavigationsToHttps(),
      _omniboxClient->GetHttpsPortForTesting(),
      _omniboxClient->IsUsingFakeHttpsForHttpsUpgradeTesting());
  AutocompleteInput& input = _omniboxTextModel->input;
  input.set_current_url(_omniboxClient->GetURL());
  input.set_current_title(_omniboxClient->GetTitle());
  input.set_prevent_inline_autocomplete(preventInlineAutocomplete);
  [self attachSuggestInputsToAutocompleteInput:input];
  [self attachAimToolModeToAutocompleteInput:input];

  [self startAutocompleteWithInput:input];
}

- (void)startZeroSuggestRequestWithText:(const std::u16string&)text
                          userClobbered:(BOOL)userClobberedPermanentText {
  if (!_autocompleteController || !_omniboxClient || !_omniboxTextModel) {
    return;
  }

  // Early exit if a query is already in progress or the popup is already open.
  // This is what allows this method to be called multiple times in multiple
  // code locations without harm.
  if (!_autocompleteController->done() || self.hasSuggestions) {
    return;
  }

  // Early exit if the page has not loaded yet, so we don't annoy users.
  if (!_omniboxClient->CurrentPageExists()) {
    return;
  }

  // Early exit if the user already has a navigation or search query in mind.
  if (_omniboxTextModel->user_input_in_progress &&
      !userClobberedPermanentText) {
    return;
  }

  TRACE_EVENT0("omnibox",
               "OmniboxTextController::startZeroSuggestRequestWithClobber");

  // Send the textfield contents exactly as-is, as otherwise the verbatim
  // match can be wrong. The full page URL is anyways in set_current_url().
  // Don't attempt to use https as the default scheme for these requests.
  _omniboxTextModel->input = AutocompleteInput(
      text, _omniboxClient->GetPageClassification(/*is_prefetch=*/false),
      _omniboxClient->GetSchemeClassifier(),
      /*should_use_https_as_default_scheme=*/false,
      _omniboxClient->GetHttpsPortForTesting(),
      _omniboxClient->IsUsingFakeHttpsForHttpsUpgradeTesting());
  AutocompleteInput& input = _omniboxTextModel->input;
  input.set_current_url(_omniboxClient->GetURL());
  input.set_current_title(_omniboxClient->GetTitle());
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  [self attachSuggestInputsToAutocompleteInput:input];
  [self attachAimToolModeToAutocompleteInput:input];

  [self startAutocompleteWithInput:input];
}

- (void)resetSession {
  if (_autocompleteController) {
    _autocompleteController->ResetSession();
  }
}

- (BOOL)findMatchForInput:(const AutocompleteInput&)input
                     match:(AutocompleteMatch*)match
    alternateNavigationURL:(GURL*)alternateNavigationURL {
  // If there's a query in progress or the popup is open, pick out the default
  // match or selected match, if there is one.
  BOOL foundMatch = NO;
  if (_autocompleteController &&
      (!_autocompleteController->done() || self.hasSuggestions)) {
    if (!_autocompleteController->done() &&
        _autocompleteController->result().default_match()) {
      // The user cannot have manually selected a match, or the query would have
      // stopped. So the default match must be the desired selection.
      *match = *_autocompleteController->result().default_match();
      foundMatch = YES;
      if (alternateNavigationURL) {
        *alternateNavigationURL = [self computeAlternateNavURLForInput:input
                                                                 match:*match];
      }
    }
  }
  return foundMatch;
}

- (GURL)computeAlternateNavURLForInput:(const AutocompleteInput&)input
                                 match:(const AutocompleteMatch&)match {
  if (!_autocompleteController) {
    return GURL();
  }
  AutocompleteProviderClient* providerClient =
      _autocompleteController->autocomplete_provider_client();
  return AutocompleteResult::ComputeAlternateNavUrl(input, match,
                                                    providerClient);
}

- (void)closeOmniboxPopup {
  [self stopAutocompleteWithClearSuggestions:YES];
}

- (void)setTextAlignment:(NSTextAlignment)alignment {
  [self.delegate omniboxAutocompleteController:self
                        didUpdateTextAlignment:alignment];
}

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  [self.delegate omniboxAutocompleteController:self
             didUpdateSemanticContentAttribute:semanticContentAttribute];
}

- (void)setHasThumbnail:(BOOL)hasThumbnail {
  self.autocompleteResultWrapper.hasThumbnail = hasThumbnail;
}

- (void)previewSuggestion:(id<AutocompleteSuggestion>)suggestion
            isFirstUpdate:(BOOL)isFirstUpdate {
  [self.omniboxTextController previewSuggestion:suggestion
                                  isFirstUpdate:isFirstUpdate];
}

- (const AutocompleteResult*)autocompleteResult {
  return _autocompleteController ? &_autocompleteController->result() : nullptr;
}

#pragma mark - Prefetch events

- (void)startZeroSuggestPrefetch {
  TRACE_EVENT0("omnibox",
               "OmniboxAutocompleteController::StartZeroSuggestPrefetch");

  if (!_autocompleteController || !_omniboxClient) {
    return;
  }

  auto page_classification =
      _omniboxClient->GetPageClassification(/*is_prefetch=*/true);
  GURL currentURL = _omniboxClient->GetURL();
  std::u16string text = base::UTF8ToUTF16(currentURL.spec());

  if (omnibox::IsNTPPage(page_classification)) {
    text.clear();
  }

  AutocompleteInput input(text, page_classification,
                          _omniboxClient->GetSchemeClassifier());
  input.set_current_url(currentURL);
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);
  _autocompleteController->StartPrefetch(input);
}

- (void)setBackgroundStateForProviders:(BOOL)inBackground {
  if (_autocompleteController) {
    _autocompleteController->autocomplete_provider_client()
        ->set_in_background_state(inBackground);
  }
}

#pragma mark - Private

- (void)attachAimToolModeToAutocompleteInput:(AutocompleteInput&)input {
  if (_omniboxPresentationContext != OmniboxPresentationContext::kComposebox ||
      !_omniboxClient) {
    return;
  }

  input.set_aim_tool_mode(_omniboxClient->AimToolMode());
}

/// Attaches the client's suggest inputs if valid.
- (void)attachSuggestInputsToAutocompleteInput:(AutocompleteInput&)input {
  if (!_omniboxClient) {
    return;
  }

  std::optional<lens::proto::LensOverlaySuggestInputs> suggestInputs =
      _omniboxClient->GetLensOverlaySuggestInputs();

  if (!suggestInputs ||
      _omniboxClient->AimToolMode() !=
          omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED) {
    return;
  }

  if ((_omniboxPresentationContext ==
       OmniboxPresentationContext::kComposebox) &&
      !AreLensSuggestInputsReady(suggestInputs)) {
    return;
  }

  input.set_lens_overlay_suggest_inputs(*suggestInputs);
}

/// Wraps the suggestions and send them to the delegate.
- (void)updateWithSortedResults:(const AutocompleteResult&)results {
  NSArray<id<AutocompleteSuggestionGroup>>* suggestionGroups =
      [self.autocompleteResultWrapper wrapAutocompleteResultInGroups:results];
  [self.delegate omniboxAutocompleteController:self
                    didUpdateSuggestionsGroups:suggestionGroups];
}

/// Starts autocomplete with `input`.
- (void)startAutocompleteWithInput:(const AutocompleteInput&)input {
  if (_autocompleteController) {
    TRACE_EVENT0("omnibox", "OmniboxAutocompleteController::StartAutocomplete");
    _autocompleteController->Start(input);
  }
}

#pragma mark Open match

/// Opens a match created outside of autocomplete controller.
- (void)openCustomMatch:(std::optional<AutocompleteMatch>)match
            disposition:(WindowOpenDisposition)disposition
     selectionTimestamp:(base::TimeTicks)timestamp {
  AutocompleteController* autocompleteController = self.autocompleteController;
  if (!autocompleteController || !match) {
    return;
  }
  OmniboxPopupSelection selection(
      autocompleteController->InjectAdHocMatch(match.value()));
  [self openSelection:selection timestamp:timestamp disposition:disposition];
}

/// Asks the browser to load the popup's currently selected item, using the
/// supplied disposition.  This may close the popup.
- (void)acceptInputWithDisposition:(WindowOpenDisposition)disposition
                         timestamp:(base::TimeTicks)timestamp {
  // Get the URL and transition type for the selected entry.
  GURL alternate_nav_url;
  AutocompleteMatch match =
      [self.omniboxTextController currentMatch:&alternate_nav_url];

  if (!match.destination_url.is_valid()) {
    return;
  }

  if (_omniboxTextModel->paste_state != OmniboxPasteState::kNone &&
      match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED) {
    // When the user pasted in a URL and hit enter, score it like a link click
    // rather than a normal typed URL, so it doesn't get inline autocompleted
    // as aggressively later.
    match.transition = ui::PAGE_TRANSITION_LINK;
  }

  [self openMatch:match
               popupSelection:OmniboxPopupSelection(
                                  OmniboxPopupSelection::kNoMatch)
        windowOpenDisposition:disposition
              alternateNavURL:alternate_nav_url
                   pastedText:u""
      matchSelectionTimestamp:timestamp];
}

/// Asks the browser to load `match` or execute one of its actions
/// according to `selection`.
///
/// openMatch: needs to know the original text that drove this action.  If
/// `pastedText` is non-empty, this is a Paste-And-Go/Search action, and
/// that's the relevant input text.  Otherwise, the relevant input text is
/// either the user text or the display URL, depending on if user input is
/// in progress.
///
/// `match` is passed by value for two reasons:
/// (1) This function needs to modify `match`, so a const ref isn't
///     appropriate.  Callers don't actually care about the modifications, so a
///     pointer isn't required.
/// (2) The passed-in match is, on the caller side, typically coming from data
///     associated with the popup.  Since this call can close the popup, that
///     could clear that data, leaving us with a pointer-to-garbage.  So at
///     some point someone needs to make a copy of the match anyway, to
///     preserve it past the popup closure.
- (void)openMatch:(AutocompleteMatch)match
             popupSelection:(OmniboxPopupSelection)selection
      windowOpenDisposition:(WindowOpenDisposition)disposition
            alternateNavURL:(const GURL&)alternateNavURL
                 pastedText:(const std::u16string&)pastedText
    matchSelectionTimestamp:(base::TimeTicks)matchSelectionTimestamp {
  // If the user is executing an action, this will be non-null and some match
  // opening and metrics behavior will be adjusted accordingly.
  OmniboxAction* action = nullptr;
  if (selection.state == OmniboxPopupSelection::NORMAL &&
      match.takeover_action) {
    DCHECK(matchSelectionTimestamp != base::TimeTicks());
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
  bool isTabSwitchAction =
      action && action->ActionId() == OmniboxActionId::TAB_SWITCH;
  if (isTabSwitchAction) {
    disposition = WindowOpenDisposition::SWITCH_TO_TAB;
  }

  TRACE_EVENT("omnibox", "OmniboxAutocompleteController::OpenMatch", "match",
              match, "disposition", disposition, "altenate_nav_url",
              alternateNavURL, "pasted_text", pastedText);

  // Update the match with the final destination URL.
  const BOOL isPastedText = !pastedText.empty();
  base::TimeDelta elapsedTimeSinceUserFirstModifiedOmnibox =
      [self.omniboxMetricsRecorder
          elapsedTimeSinceUserFirstModifiedOmniboxWithPastedText:isPastedText];
  self.autocompleteController
      ->UpdateMatchDestinationURLWithAdditionalSearchboxStats(
          elapsedTimeSinceUserFirstModifiedOmnibox, &match);

  GURL destinationURL = action ? action->getUrl() : match.destination_url;

  std::u16string inputText(pastedText);
  if (inputText.empty()) {
    inputText = _omniboxTextModel->user_input_in_progress
                    ? _omniboxTextModel->user_text
                    : _omniboxTextModel->url_for_editing;
  }

  // Save the result of the interaction, but do not record the histogram yet.
  _omniboxTextModel->focus_resulted_in_navigation = true;

  // Create a dummy AutocompleteInput for use in calling VerbatimMatchForInput()
  // to create an alternate navigational match.
  AutocompleteInput alternateInput(
      inputText, _omniboxClient->GetPageClassification(/*is_prefetch=*/false),
      _omniboxClient->GetSchemeClassifier(),
      _omniboxClient->ShouldDefaultTypedNavigationsToHttps(), 0, false);
  // Somehow we can occasionally get here with no active tab.  It's not
  // clear why this happens.
  alternateInput.set_current_url(_omniboxClient->GetURL());
  alternateInput.set_current_title(_omniboxClient->GetTitle());

  [self.omniboxMetricsRecorder recordOpenMatch:match
                                destinationURL:destinationURL
                                     inputText:inputText
                                popupSelection:selection
                         windowOpenDisposition:disposition
                                      isAction:action
                                  isPastedText:isPastedText];

  if (action) {
    OmniboxAction::ExecutionContext context(
        *(self.autocompleteController->autocomplete_provider_client()),
        base::BindOnce(&OmniboxClient::OnAutocompleteAccept,
                       _omniboxClient->AsWeakPtr()),
        matchSelectionTimestamp, disposition);
    action->Execute(context);
  }

  if (disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    // Skip the revert here to avoid changing the size of the multiline omnibox
    // when accepting input, changes will be reverted at endEditing
    // (crbug.com/458055336).
    BOOL skipRevert = (IsMultilineBrowserOmniboxEnabled() &&
                       _omniboxPresentationContext ==
                           OmniboxPresentationContext::kLocationBar) ||
                      (IsComposeboxIOSEnabled() &&
                       _omniboxPresentationContext ==
                           OmniboxPresentationContext::kComposebox);
    if (!skipRevert) {
      base::AutoReset<bool> tmp(&_omniboxTextModel->in_revert, true);
      [self.omniboxTextController
              revertAll];  // Revert the box to its unedited state.
    }
  }

  if (!action) {
    // This block should be the last call in openMatch, because controller_ is
    // not guaranteed to exist after client()->OnAutocompleteAccept is called.
    if (destinationURL.is_valid()) {
      // This calls RevertAll again.
      base::AutoReset<bool> tmp(&_omniboxTextModel->in_revert, true);

      _omniboxClient->OnAutocompleteAccept(
          destinationURL, match.post_content.get(), disposition,
          ui::PageTransitionFromInt(match.transition |
                                    ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
          match.type, matchSelectionTimestamp,
          _omniboxTextModel->input.added_default_scheme_to_typed_url(),
          _omniboxTextModel->input.typed_url_had_http_scheme() &&
              match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED,
          inputText, match,
          VerbatimMatchForInput(
              self.autocompleteController->history_url_provider(),
              self.autocompleteController->autocomplete_provider_client(),
              alternateInput, alternateNavURL, false));
    }
  }
}

#pragma mark Clipboard match handling

/// Creates a match with the clipboard URL and open it.
- (void)openClipboardURL:(std::optional<GURL>)optionalURL
             disposition:(WindowOpenDisposition)disposition
               timestamp:(base::TimeTicks)timestamp {
  if (!optionalURL || !_autocompleteController) {
    return;
  }
  GURL URL = std::move(optionalURL).value();
  [self openCustomMatch:_autocompleteController->clipboard_provider()
                            ->NewClipboardURLMatch(URL)
             disposition:disposition
      selectionTimestamp:timestamp];
}

/// Creates a match with the clipboard text and open it.
- (void)openClipboardText:(std::optional<std::u16string>)optionalText
              disposition:(WindowOpenDisposition)disposition
                timestamp:(base::TimeTicks)timestamp {
  if (!optionalText || !_autocompleteController) {
    return;
  }
  [self openCustomMatch:_autocompleteController->clipboard_provider()
                            ->NewClipboardTextMatch(optionalText.value())
             disposition:disposition
      selectionTimestamp:timestamp];
}

/// Creates a match with the clipboard image and open it.
- (void)openClipboardImage:(std::optional<gfx::Image>)optionalImage
               disposition:(WindowOpenDisposition)disposition
                 timestamp:(base::TimeTicks)timestamp {
  if (!optionalImage || !_autocompleteController) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _autocompleteController->clipboard_provider()->NewClipboardImageMatch(
      optionalImage,
      base::BindOnce(
          [](OmniboxAutocompleteController* controller,
             WindowOpenDisposition disposition, base::TimeTicks timestamp,
             std::optional<AutocompleteMatch> optionalMatch) {
            [controller openCustomMatch:optionalMatch
                            disposition:disposition
                     selectionTimestamp:timestamp];
          },
          weakSelf, disposition, timestamp));
}

/// Opens a clipboard match. Fetches the content of the clipboard and creates a
/// new match with it.
- (void)openClipboardMatch:(const AutocompleteMatch&)match
               disposition:(WindowOpenDisposition)disposition
        selectionTimestamp:(base::TimeTicks)timestamp {
  __weak __typeof__(self) weakSelf = self;
  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();
  CHECK(clipboardRecentContent);

  switch (match.type) {
    case AutocompleteMatchType::CLIPBOARD_URL: {
      clipboardRecentContent->GetRecentURLFromClipboard(base::BindOnce(
          [](OmniboxAutocompleteController* controller,
             WindowOpenDisposition disposition, base::TimeTicks timestamp,
             std::optional<GURL> optionalURL) {
            [controller openClipboardURL:optionalURL
                             disposition:disposition
                               timestamp:timestamp];
          },
          weakSelf, disposition, timestamp));
      break;
    }
    case AutocompleteMatchType::CLIPBOARD_TEXT: {
      clipboardRecentContent->GetRecentTextFromClipboard(base::BindOnce(
          [](OmniboxAutocompleteController* controller,
             WindowOpenDisposition disposition, base::TimeTicks timestamp,
             std::optional<std::u16string> optionalText) {
            [controller openClipboardText:optionalText
                              disposition:disposition
                                timestamp:timestamp];
          },
          weakSelf, disposition, timestamp));
      break;
    }
    case AutocompleteMatchType::CLIPBOARD_IMAGE: {
      if ([self.lensHander shouldUseLensForCopiedImage]) {
        [self.lensHander lensCopiedImage];
      } else {
        clipboardRecentContent->GetRecentImageFromClipboard(base::BindOnce(
            [](OmniboxAutocompleteController* controller,
               WindowOpenDisposition disposition, base::TimeTicks timestamp,
               std::optional<gfx::Image> optionalImage) {
              [controller openClipboardImage:optionalImage
                                 disposition:disposition
                                   timestamp:timestamp];
            },
            weakSelf, disposition, timestamp));
      }

      break;
    }
    default:
      NOTREACHED() << "Unsupported clipboard match type";
  }
}

#pragma mark - Testing

- (void)setAutocompleteController:(AutocompleteController*)controller {
  CHECK(_autocompleteControllerObserverBridge);
  // Remove observation on old controller.
  if (_autocompleteController) {
    _autocompleteController->RemoveObserver(
        _autocompleteControllerObserverBridge.get());
  }
  // Set new controller.
  _autocompleteController = controller;
  // Observe new controller.
  if (_autocompleteController) {
    _autocompleteController->AddObserver(
        _autocompleteControllerObserverBridge.get());

    // Update the autocomplete controller in the metrics recorder and the text
    // controller.
    [self.omniboxMetricsRecorder
        setAutocompleteController:_autocompleteController];
  }
}

@end
