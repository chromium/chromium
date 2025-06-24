// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"

#import <optional>
#import <string>

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/trace_event/trace_event.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/clipboard_provider.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/omnibox/browser/page_classification_functions.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_controller_observer_bridge.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_result_wrapper.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller_debugger_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "third_party/omnibox_proto/groups.pb.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

using base::UserMetricsAction;

@interface OmniboxAutocompleteController () <AutocompleteControllerObserver,
                                             BooleanObserver>

/// Redefined as a readwrite
@property(nonatomic, assign, readwrite) BOOL hasSuggestions;

/// Autocomplete controller is accessed from OmniboxController. It might be
/// changed by `SetAutocompleteControllerForTesting`.
@property(nonatomic, assign, readonly)
    AutocompleteController* autocompleteController;

@end

@implementation OmniboxAutocompleteController {
  /// Client of the omnibox.
  raw_ptr<OmniboxClient> _omniboxClient;
  /// Controller of the omnibox.
  raw_ptr<OmniboxControllerIOS> _omniboxController;
  /// Omnibox edit model. Should only be used for autocomplete interactions.
  raw_ptr<OmniboxEditModelIOS> _omniboxEditModel;
  /// Omnibox text model.
  raw_ptr<OmniboxTextModel> _omniboxTextModel;

  /// Autocomplete controller observer.
  std::unique_ptr<AutocompleteControllerObserverBridge>
      _autocompleteControllerObserverBridge;
  /// Pref tracking if the bottom omnibox is enabled.
  PrefBackedBoolean* _bottomOmniboxEnabled;
  /// Preferred omnibox position, logged in omnibox logs.
  metrics::OmniboxEventProto::OmniboxPosition _preferredOmniboxPosition;
}

- (instancetype)initWithOmniboxController:
                    (OmniboxControllerIOS*)omniboxController
                            omniboxClient:(OmniboxClient*)omniboxClient
                         omniboxEditModel:(OmniboxEditModelIOS*)omniboxEditModel
                         omniboxTextModel:(OmniboxTextModel*)omniboxTextModel {
  self = [super init];
  if (self) {
    _omniboxClient = omniboxClient;
    _omniboxController = omniboxController;
    _omniboxEditModel = omniboxEditModel;
    _omniboxTextModel = omniboxTextModel;

    _autocompleteControllerObserverBridge =
        std::make_unique<AutocompleteControllerObserverBridge>(self);
    if (_omniboxController && _omniboxController->autocomplete_controller()) {
      _omniboxController->autocomplete_controller()->AddObserver(
          _autocompleteControllerObserverBridge.get());
    }
    _preferredOmniboxPosition = metrics::OmniboxEventProto::UNKNOWN_POSITION;
    _bottomOmniboxEnabled = [[PrefBackedBoolean alloc]
        initWithPrefService:GetApplicationContext()->GetLocalState()
                   prefName:prefs::kBottomOmnibox];
    [_bottomOmniboxEnabled setObserver:self];
    // Initialize to the correct value.
    [self booleanDidChange:_bottomOmniboxEnabled];
  }
  return self;
}

- (void)disconnect {
  if (_autocompleteControllerObserverBridge && _omniboxController &&
      _omniboxController->autocomplete_controller()) {
    _omniboxController->autocomplete_controller()->RemoveObserver(
        _autocompleteControllerObserverBridge.get());
    _autocompleteControllerObserverBridge.reset();
  }
  [self.autocompleteResultWrapper disconnect];
  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;
  _autocompleteResultWrapper = nil;
  _omniboxEditModel = nullptr;
  _omniboxController = nullptr;
  _omniboxTextModel = nullptr;
  _omniboxClient = nullptr;
}

- (AutocompleteController*)autocompleteController {
  return _omniboxController ? _omniboxController->autocomplete_controller()
                            : nullptr;
}

- (void)updatePopupSuggestions {
  if (AutocompleteController* autocompleteController =
          self.autocompleteController) {
    BOOL isFocusing = autocompleteController->input().focus_type() ==
                      metrics::OmniboxFocusType::INTERACTION_FOCUS;

    self.hasSuggestions = !autocompleteController->result().empty();
    [self.delegate
        omniboxAutocompleteControllerDidUpdateSuggestions:self
                                           hasSuggestions:self.hasSuggestions
                                               isFocusing:isFocusing];
    [self.debuggerDelegate omniboxAutocompleteController:self
                       didUpdateWithSuggestionsAvailable:self.hasSuggestions];
  }
}

- (void)stopAutocompleteWithClearSuggestions:(BOOL)clearSuggestions {
  TRACE_EVENT0("omnibox", "OmniboxAutocompleteController::StopAutocomplete");
  if (AutocompleteController* autocompleteController =
          self.autocompleteController) {
    autocompleteController->Stop(clearSuggestions
                                     ? AutocompleteStopReason::kClobbered
                                     : AutocompleteStopReason::kInteraction);
  }
}

#pragma mark - AutocompleteControllerObserver

- (void)autocompleteController:(AutocompleteController*)autocompleteController
    didUpdateResultChangingDefaultMatch:(BOOL)defaultMatchChanged {
  TRACE_EVENT0("omnibox", "OmniboxAutocompleteController::OnResultChanged");
  DCHECK(autocompleteController == self.autocompleteController);
  DCHECK(_omniboxClient);

  const bool popup_was_open = _omniboxEditModel->PopupIsOpen();

  [self updatePopupSuggestions];
  if (defaultMatchChanged) {
    // The default match has changed, we need to let the OmniboxEditModelIOS
    // know about new inline autocomplete text (blue highlight).
    if (const AutocompleteMatch* match =
            autocompleteController->result().default_match()) {
      // OnPopupDataChanged() resets edit model's `current_match_` early
      // on.  Therefore, copy match.inline_autocompletion to a temp to preserve
      // its value across the entire call.
      _omniboxEditModel->OnPopupDataChanged(match->inline_autocompletion,
                                            match->additional_text, *match);
    } else {
      _omniboxEditModel->OnPopupDataChanged(std::u16string(), std::u16string(),
                                            AutocompleteMatch());
    }
  }

  const bool popup_is_open = _omniboxEditModel->PopupIsOpen();
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
    _omniboxEditModel->ClearAdditionalText();
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
    if (AutocompleteController* autocompleteController =
            self.autocompleteController) {
      autocompleteController->SetSteadyStateOmniboxPosition(
          _preferredOmniboxPosition);
    }
  }
}

#pragma mark - OmniboxPopup event

- (void)requestSuggestionsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount {
  AutocompleteController* autocompleteController = self.autocompleteController;
  if (!autocompleteController) {
    return;
  }
  size_t resultSize = autocompleteController->result().size();
  // If no suggestions are visible, consider all of them visible.
  if (visibleSuggestionCount == 0) {
    visibleSuggestionCount = resultSize;
  }
  NSUInteger visibleSuggestions = MIN(visibleSuggestionCount, resultSize);
  if (visibleSuggestions > 0) {
    // Groups visible suggestions by search vs url. Skip the first suggestion
    // because it's the omnibox content.
    autocompleteController->GroupSuggestionsBySearchVsURL(1,
                                                          visibleSuggestions);
  }
  // Groups hidden suggestions by search vs url.
  if (visibleSuggestions < resultSize) {
    autocompleteController->GroupSuggestionsBySearchVsURL(visibleSuggestions,
                                                          resultSize);
  }

  [self updateWithSortedResults:autocompleteController->result()];
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

  AutocompleteController* autocompleteController = self.autocompleteController;
  if (!autocompleteController || !_omniboxEditModel) {
    return;
  }

  // Sometimes the match provided does not correspond to the autocomplete
  // result match specified by `index`. Most Visited Tiles, for example,
  // provide ad hoc matches that are not in the result at all.
  if (row >= autocompleteController->result().size() ||
      autocompleteController->result().match_at(row).destination_url !=
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

  if (_omniboxEditModel) {
    _omniboxEditModel->OpenSelection(OmniboxPopupSelection(row),
                                     matchSelectionTimestamp, disposition);
  }
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
  if (AutocompleteController* autocompleteController =
          self.autocompleteController) {
    autocompleteController->DeleteMatch(match);
  }
}

- (void)onScroll {
  [self.omniboxTextController onScroll];
}

- (void)onCallAction {
  [self.omniboxTextController hideKeyboard];
}

#pragma mark - OmniboxText events

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
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggestInputs =
          _omniboxClient->GetLensOverlaySuggestInputs()) {
    input.set_lens_overlay_suggest_inputs(*suggestInputs);
  }

  [self startAutocompleteWithInput:input];
}

- (void)startZeroSuggestRequestWithText:(const std::u16string&)text
                          userClobbered:(BOOL)userClobberedPermanentText {
  AutocompleteController* autocompleteController = self.autocompleteController;
  if (!autocompleteController || !_omniboxClient || !_omniboxTextModel) {
    return;
  }

  // Early exit if a query is already in progress or the popup is already open.
  // This is what allows this method to be called multiple times in multiple
  // code locations without harm.
  if (!autocompleteController->done() || self.hasSuggestions) {
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
  // Set the lens overlay suggest inputs, if available.
  if (std::optional<lens::proto::LensOverlaySuggestInputs> suggestInputs =
          _omniboxClient->GetLensOverlaySuggestInputs()) {
    input.set_lens_overlay_suggest_inputs(*suggestInputs);
  }
  [self startAutocompleteWithInput:input];
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

#pragma mark - Prefetch events

- (void)startZeroSuggestPrefetch {
  TRACE_EVENT0("omnibox",
               "OmniboxAutocompleteController::StartZeroSuggestPrefetch");

  AutocompleteController* autocompleteController = self.autocompleteController;
  if (!autocompleteController || !_omniboxClient) {
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
  autocompleteController->StartPrefetch(input);
}

- (void)setBackgroundStateForProviders:(BOOL)inBackground {
  if (AutocompleteController* autocompleteController =
          self.autocompleteController) {
    autocompleteController->autocomplete_provider_client()
        ->set_in_background_state(inBackground);
  }
}

#pragma mark - Private

/// Opens a match created outside of autocomplete controller.
- (void)openCustomMatch:(std::optional<AutocompleteMatch>)match
            disposition:(WindowOpenDisposition)disposition
     selectionTimestamp:(base::TimeTicks)timestamp {
  AutocompleteController* autocompleteController = self.autocompleteController;
  if (!autocompleteController || !_omniboxEditModel || !match) {
    return;
  }
  OmniboxPopupSelection selection(
      autocompleteController->InjectAdHocMatch(match.value()));
  _omniboxEditModel->OpenSelection(selection, timestamp, disposition);
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
  TRACE_EVENT0("omnibox", "OmniboxAutocompleteController::StartAutocomplete");

  if (AutocompleteController* autocompleteController =
          self.autocompleteController) {
    autocompleteController->Start(input);
  }
}

#pragma mark Clipboard match handling

/// Creates a match with the clipboard URL and open it.
- (void)openClipboardURL:(std::optional<GURL>)optionalURL
             disposition:(WindowOpenDisposition)disposition
               timestamp:(base::TimeTicks)timestamp {
  if (!optionalURL) {
    return;
  }
  GURL URL = std::move(optionalURL).value();
  if (AutocompleteController* autocompleteController =
          self.autocompleteController) {
    [self openCustomMatch:autocompleteController->clipboard_provider()
                              ->NewClipboardURLMatch(URL)
               disposition:disposition
        selectionTimestamp:timestamp];
  }
}

/// Creates a match with the clipboard text and open it.
- (void)openClipboardText:(std::optional<std::u16string>)optionalText
              disposition:(WindowOpenDisposition)disposition
                timestamp:(base::TimeTicks)timestamp {
  if (!optionalText) {
    return;
  }
  if (AutocompleteController* autocompleteController =
          self.autocompleteController) {
    [self openCustomMatch:autocompleteController->clipboard_provider()
                              ->NewClipboardTextMatch(optionalText.value())
               disposition:disposition
        selectionTimestamp:timestamp];
  }
}

/// Creates a match with the clipboard image and open it.
- (void)openClipboardImage:(std::optional<gfx::Image>)optionalImage
               disposition:(WindowOpenDisposition)disposition
                 timestamp:(base::TimeTicks)timestamp {
  if (!optionalImage) {
    return;
  }

  if (AutocompleteController* autocompleteController =
          self.autocompleteController) {
    __weak __typeof(self) weakSelf = self;
    autocompleteController->clipboard_provider()->NewClipboardImageMatch(
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
      clipboardRecentContent->GetRecentImageFromClipboard(base::BindOnce(
          [](OmniboxAutocompleteController* controller,
             WindowOpenDisposition disposition, base::TimeTicks timestamp,
             std::optional<gfx::Image> optionalImage) {
            [controller openClipboardImage:optionalImage
                               disposition:disposition
                                 timestamp:timestamp];
          },
          weakSelf, disposition, timestamp));
      break;
    }
    default:
      NOTREACHED() << "Unsupported clipboard match type";
  }
}

#pragma mark - Testing

- (void)setAutocompleteController:
    (std::unique_ptr<AutocompleteController>)controller {
  CHECK(_autocompleteControllerObserverBridge);

  if (!_omniboxController) {
    return;
  }

  // Remove observation on old controller.
  if (AutocompleteController* autocompleteController =
          self.autocompleteController) {
    autocompleteController->RemoveObserver(
        _autocompleteControllerObserverBridge.get());
  }
  // Set new controller.
  _omniboxController->SetAutocompleteControllerForTesting(
      std::move(controller));
  // Observe new controller.
  if (AutocompleteController* autocompleteController =
          self.autocompleteController) {
    autocompleteController->AddObserver(
        _autocompleteControllerObserverBridge.get());
  }
}

@end
