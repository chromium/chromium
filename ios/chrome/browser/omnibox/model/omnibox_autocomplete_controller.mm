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
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"
#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_view_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "third_party/omnibox_proto/groups.pb.h"
#import "ui/gfx/image/image.h"
#import "url/gurl.h"

using base::UserMetricsAction;

@interface OmniboxAutocompleteController () <BooleanObserver>

@end

@implementation OmniboxAutocompleteController {
  /// Controller of the omnibox.
  raw_ptr<OmniboxController> _omniboxController;
  /// Controller of autocomplete.
  raw_ptr<AutocompleteController> _autocompleteController;
  /// Controller of the omnibox view.
  raw_ptr<OmniboxViewIOS> _omniboxViewIOS;
  /// Omnibox edit model. Should only be used for autocomplete interactions.
  raw_ptr<OmniboxEditModel> _omniboxEditModel;

  /// Pref tracking if the bottom omnibox is enabled.
  PrefBackedBoolean* _bottomOmniboxEnabled;
  /// Preferred omnibox position, logged in omnibox logs.
  metrics::OmniboxEventProto::OmniboxPosition _preferredOmniboxPosition;
}

- (instancetype)initWithOmniboxController:(OmniboxController*)omniboxController
                           omniboxViewIOS:(OmniboxViewIOS*)omniboxViewIOS {
  self = [super init];
  if (self) {
    _omniboxController = omniboxController;
    _autocompleteController = omniboxController->autocomplete_controller();
    _omniboxViewIOS = omniboxViewIOS;
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
  [_bottomOmniboxEnabled stop];
  [_bottomOmniboxEnabled setObserver:nil];
  _bottomOmniboxEnabled = nil;
  _autocompleteController = nullptr;
  _omniboxEditModel = nullptr;
  _omniboxController = nullptr;
  _omniboxViewIOS = nullptr;
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _bottomOmniboxEnabled) {
    _preferredOmniboxPosition =
        _bottomOmniboxEnabled.value
            ? metrics::OmniboxEventProto::BOTTOM_POSITION
            : metrics::OmniboxEventProto::TOP_POSITION;
    if (_autocompleteController) {
      _autocompleteController->SetSteadyStateOmniboxPosition(
          _preferredOmniboxPosition);
    }
  }
}

#pragma mark - OmniboxEditModel event

- (void)updatePopupSuggestions {
  if (_autocompleteController) {
    BOOL isFocusing = _autocompleteController->input().focus_type() ==
                      metrics::OmniboxFocusType::INTERACTION_FOCUS;
    [self.omniboxPopupController
        newResultsAvailable:_autocompleteController->result()
                 isFocusing:isFocusing];
  }
}

#pragma mark - OmniboxPopup event

- (void)requestResultsWithVisibleSuggestionCount:
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

  [self.omniboxPopupController
      updateWithSortedResults:_autocompleteController->result()];
}

- (BOOL)isStarredMatch:(const AutocompleteMatch&)match {
  if (_omniboxController && _omniboxController->client()) {
    auto* bookmark_model = _omniboxController->client()->GetBookmarkModel();
    return bookmark_model &&
           bookmark_model->IsBookmarked(match.destination_url);
  }
  return NO;
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

  if (!_autocompleteController || !_omniboxEditModel) {
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

  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnSelectedMatchForAppending(fill_into_edit);
  }
}

- (void)selectMatchForDeletion:(const AutocompleteMatch&)match {
  if (_autocompleteController) {
    _autocompleteController->DeleteMatch(match);
  }
}

- (void)onScroll {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnPopupDidScroll();
  }
}

- (void)onCallAction {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnCallActionTap();
  }
}

#pragma mark - Private

/// Opens a match created outside of autocomplete controller.
- (void)openCustomMatch:(std::optional<AutocompleteMatch>)match
            disposition:(WindowOpenDisposition)disposition
     selectionTimestamp:(base::TimeTicks)timestamp {
  if (!_autocompleteController || !_omniboxEditModel || !match) {
    return;
  }
  OmniboxPopupSelection selection(
      _autocompleteController->InjectAdHocMatch(match.value()));
  _omniboxEditModel->OpenSelection(selection, timestamp, disposition);
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
