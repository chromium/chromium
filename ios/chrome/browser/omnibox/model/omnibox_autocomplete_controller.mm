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
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/clipboard_provider.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_result_wrapper.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller_debugger_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "third_party/omnibox_proto/groups.pb.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

using base::UserMetricsAction;

@interface OmniboxAutocompleteController () <BooleanObserver>

/// Redefined as a readwrite
@property(nonatomic, assign, readwrite) BOOL hasSuggestions;

/// Autocomplete controller is accessed from OmniboxController. It might be
/// changed by `SetAutocompleteControllerForTesting`.
@property(nonatomic, assign, readonly)
    AutocompleteController* autocompleteController;

@end

@implementation OmniboxAutocompleteController {
  /// Controller of the omnibox.
  raw_ptr<OmniboxControllerIOS> _omniboxController;
  /// Omnibox edit model. Should only be used for autocomplete interactions.
  raw_ptr<OmniboxEditModelIOS> _omniboxEditModel;

  /// Pref tracking if the bottom omnibox is enabled.
  PrefBackedBoolean* _bottomOmniboxEnabled;
  /// Preferred omnibox position, logged in omnibox logs.
  metrics::OmniboxEventProto::OmniboxPosition _preferredOmniboxPosition;
}

- (instancetype)initWithOmniboxController:
    (OmniboxControllerIOS*)omniboxController {
  self = [super init];
  if (self) {
    _omniboxController = omniboxController;
    _omniboxEditModel = omniboxController->edit_model();

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
  [self.autocompleteResultWrapper disconnect];
  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;
  _autocompleteResultWrapper = nil;
  _omniboxEditModel = nullptr;
  _omniboxController = nullptr;
}

- (AutocompleteController*)autocompleteController {
  return _omniboxController ? _omniboxController->autocomplete_controller()
                            : nullptr;
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

#pragma mark - OmniboxEditModel event

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

- (void)closeOmniboxPopup {
  if (_omniboxController) {
    _omniboxController->StopAutocomplete(/*clear_result=*/true);
  }
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
  [self.delegate omniboxAutocompleteController:self
                         didUpdateHasThumbnail:hasThumbnail];
  self.autocompleteResultWrapper.hasThumbnail = hasThumbnail;
}

- (void)previewSuggestion:(id<AutocompleteSuggestion>)suggestion
            isFirstUpdate:(BOOL)isFirstUpdate {
  [self.omniboxTextController previewSuggestion:suggestion
                                  isFirstUpdate:isFirstUpdate];
}

#pragma mark - OmniboxAutocomplete event

- (void)updateWithSortedResults:(const AutocompleteResult&)results {
  NSArray<id<AutocompleteSuggestionGroup>>* suggestionGroups =
      [self.autocompleteResultWrapper wrapAutocompleteResultInGroups:results];
  [self.delegate omniboxAutocompleteController:self
                    didUpdateSuggestionsGroups:suggestionGroups];
}

#pragma mark - AutocompleteResultWrapperDelegate

- (void)autocompleteResultWrapper:(AutocompleteResultWrapper*)wrapper
              didInvalidatePedals:(NSArray<id<AutocompleteSuggestionGroup>>*)
                                      nonPedalSuggestionsGroups {
  [self.delegate omniboxAutocompleteController:self
                    didUpdateSuggestionsGroups:nonPedalSuggestionsGroups];
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

@end
