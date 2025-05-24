// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/coordinator/popup/omnibox_popup_mediator.h"

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/omnibox/browser/actions/omnibox_action_concepts.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/strings/grit/components_strings.h"
#import "components/variations/variations_associated_data.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/omnibox/coordinator/popup/omnibox_popup_mediator+Testing.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_match_formatter.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_image_fetcher.h"
#import "ios/chrome/browser/omnibox/model/omnibox_pedal_swift.h"
#import "ios/chrome/browser/omnibox/model/suggest_action.h"
#import "ios/chrome/browser/omnibox/ui/popup/carousel/carousel_item.h"
#import "ios/chrome/browser/omnibox/ui/popup/carousel/carousel_item_menu_provider.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_consumer.h"
#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_omnibox_consumer.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
/// Maximum number of suggest tile types we want to record. Anything beyond this
/// will be reported in the overflow bucket.
const NSUInteger kMaxSuggestTileTypePosition = 15;
}  // namespace

@interface OmniboxPopupMediator ()

// FET reference.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;
/// Index of the group containing AutocompleteSuggestion, first group to be
/// highlighted on down arrow key.
@property(nonatomic, assign) NSInteger preselectedGroupIndex;

// Whether the omnibox has a thumbnail.
@property(nonatomic, assign) BOOL hasThumbnail;

// Holds the current suggestion groups.
@property(nonatomic, strong)
    NSArray<id<AutocompleteSuggestionGroup>>* suggestionGroups;

@end

@implementation OmniboxPopupMediator {
  /// omnibox images/favicons fetcher.
  OmniboxImageFetcher* _omniboxImageFetcher;
}
@synthesize consumer = _consumer;
@synthesize incognito = _incognito;
@synthesize presenter = _presenter;

- (instancetype)initWithTracker:(feature_engagement::Tracker*)tracker
            omniboxImageFetcher:(OmniboxImageFetcher*)omniboxImageFetcher {
  self = [super init];
  if (self) {
    _open = NO;
    _preselectedGroupIndex = 0;
    _tracker = tracker;
    _omniboxImageFetcher = omniboxImageFetcher;
  }
  return self;
}

- (void)setOpen:(BOOL)open {
  // When closing the popup.
  if (_open && !open) {
    [_omniboxImageFetcher clearCache];
  }
  _open = open;
}

#pragma mark - OmniboxAutocompleteControllerDelegate

- (void)omniboxAutocompleteControllerDidUpdateSuggestions:
            (OmniboxAutocompleteController*)autocompleteController
                                           hasSuggestions:(BOOL)hasSuggestions
                                               isFocusing:(BOOL)isFocusing {
  [self.consumer newResultsAvailable];

  self.open = hasSuggestions;
  [self.presenter updatePopupOnFocus:isFocusing];
}

- (void)omniboxAutocompleteController:
            (OmniboxAutocompleteController*)omniboxAutocompleteController
               didUpdateTextAlignment:(NSTextAlignment)alignment {
  [self.consumer setTextAlignment:alignment];
}

- (void)omniboxAutocompleteController:
            (OmniboxAutocompleteController*)omniboxAutocompleteController
    didUpdateSemanticContentAttribute:
        (UISemanticContentAttribute)semanticContentAttribute {
  [self.consumer setSemanticContentAttribute:semanticContentAttribute];
}

- (void)omniboxAutocompleteController:
            (OmniboxAutocompleteController*)omniboxAutocompleteController
                didUpdateHasThumbnail:(BOOL)hasThumbnail {
  self.hasThumbnail = hasThumbnail;
}

- (void)omniboxAutocompleteController:
            (OmniboxAutocompleteController*)omniboxAutocompleteController
           didUpdateSuggestionsGroups:
               (NSArray<id<AutocompleteSuggestionGroup>>*)suggestionGroups {
  _suggestionGroups = suggestionGroups;

  // Preselect the verbatim match. It's the top match, unless we inserted pedals
  // and pushed it one section down.
  self.preselectedGroupIndex = 0;
  if (_suggestionGroups.count > 0 &&
      _suggestionGroups[0].type == SuggestionGroupType::kPedalSuggestionGroup) {
    self.preselectedGroupIndex = 1;
  }

  [self.consumer updateMatches:_suggestionGroups
      preselectedMatchGroupIndex:self.preselectedGroupIndex];
}

#pragma mark - OmniboxPopupMutator

- (void)onTraitCollectionChange {
  [self.presenter updatePopupAfterTraitCollectionChange];
}

- (void)selectSuggestion:(id<AutocompleteSuggestion>)suggestion
                   inRow:(NSUInteger)row {
  [self logPedalShownForCurrentResult];

  // Log the suggest actions that were shown and not used.
  if (suggestion.actionsInSuggest.count == 0) {
    [self logActionsInSuggestShownForCurrentResult];
  }

  if (suggestion.pedal) {
    if (suggestion.pedal.action) {
      base::UmaHistogramEnumeration("Omnibox.SuggestionUsed.Pedal",
                                    (OmniboxPedalId)suggestion.pedal.type,
                                    OmniboxPedalId::TOTAL_COUNT);
      if ((OmniboxPedalId)suggestion.pedal.type ==
          OmniboxPedalId::MANAGE_PASSWORDS) {
        base::UmaHistogramEnumeration(
            "PasswordManager.ManagePasswordsReferrer",
            password_manager::ManagePasswordsReferrer::kOmniboxPedalSuggestion);
      }
      suggestion.pedal.action();
    }
  } else if ([suggestion isKindOfClass:[AutocompleteMatchFormatter class]]) {
    AutocompleteMatchFormatter* autocompleteMatchFormatter =
        (AutocompleteMatchFormatter*)suggestion;
    const AutocompleteMatch& match =
        autocompleteMatchFormatter.autocompleteMatch;

    // A search using clipboard link or text is activity that should indicate a
    // user that would be interested in setting the browser as the default.
    if (match.type == AutocompleteMatchType::CLIPBOARD_URL) {
      default_browser::NotifyOmniboxURLCopyPasteAndNavigate(
          self.incognito, self.tracker, self.sceneState);
    }
    if (match.type == AutocompleteMatchType::CLIPBOARD_TEXT) {
      default_browser::NotifyOmniboxTextCopyPasteAndNavigate(self.tracker);
    }

    if (!self.incognito &&
        match.type == AutocompleteMatchType::TILE_NAVSUGGEST) {
      [self logSelectedAutocompleteTile:match];
    }
    [self.omniboxAutocompleteController
        selectMatchForOpening:match
                        inRow:row
                       openIn:WindowOpenDisposition::CURRENT_TAB];
  } else {
    DUMP_WILL_BE_NOTREACHED()
        << "Suggestion type " << NSStringFromClass(suggestion.class)
        << " not handled for selection.";
  }
}

- (void)selectSuggestionAction:(SuggestAction*)action
                    suggestion:(id<AutocompleteSuggestion>)suggestion
                         inRow:(NSUInteger)row {
  OmniboxActionInSuggest::RecordShownAndUsedMetrics(action.type,
                                                    true /* used */);

  switch (action.type) {
    case omnibox::ActionInfo_ActionType_CALL: {
      NSURL* URL = net::NSURLWithGURL(action.actionURI);
      __weak __typeof__(self) weakSelf = self;
      [[UIApplication sharedApplication] openURL:URL
                                         options:@{}
                               completionHandler:^(BOOL success) {
                                 if (success) {
                                   [weakSelf callActionTapped];
                                 }
                               }];
      break;
    }
    case omnibox::ActionInfo_ActionType_DIRECTIONS: {
      NSURL* URL = net::NSURLWithGURL(action.actionURI);

      if (IsGoogleMapsAppInstalled() && !self.incognito) {
        [[UIApplication sharedApplication] openURL:URL
                                           options:@{}
                                 completionHandler:nil];
      } else {
        [self openNewTabWithSuggestAction:action];
      }
      break;
    }
    case omnibox::ActionInfo_ActionType_REVIEWS: {
      [self openNewTabWithSuggestAction:action];
      break;
    }
    default:
      break;
  }
}

- (void)tapTrailingButtonOnSuggestion:(id<AutocompleteSuggestion>)suggestion
                                inRow:(NSUInteger)row {
  if ([suggestion isKindOfClass:[AutocompleteMatchFormatter class]]) {
    AutocompleteMatchFormatter* autocompleteMatchFormatter =
        (AutocompleteMatchFormatter*)suggestion;
    const AutocompleteMatch& match =
        autocompleteMatchFormatter.autocompleteMatch;
    if (match.has_tab_match.value_or(false)) {
      [self.omniboxAutocompleteController
          selectMatchForOpening:match
                          inRow:row
                         openIn:WindowOpenDisposition::SWITCH_TO_TAB];
    } else {
      if (AutocompleteMatch::IsSearchType(match.type)) {
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxRefineSuggestion.Search"));
      } else {
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxRefineSuggestion.Url"));
      }
      [self.omniboxAutocompleteController selectMatchForAppending:match];
    }
  } else {
    NOTREACHED() << "Suggestion type " << NSStringFromClass(suggestion.class)
                 << " not handled for trailing button tap.";
  }
}

- (void)selectSuggestionForDeletion:(id<AutocompleteSuggestion>)suggestion
                              inRow:(NSUInteger)row {
  if ([suggestion isKindOfClass:[AutocompleteMatchFormatter class]]) {
    AutocompleteMatchFormatter* autocompleteMatchFormatter =
        (AutocompleteMatchFormatter*)suggestion;
    const AutocompleteMatch& match =
        autocompleteMatchFormatter.autocompleteMatch;
    [self.omniboxAutocompleteController selectMatchForDeletion:match];
  } else {
    DUMP_WILL_BE_NOTREACHED()
        << "Suggestion type " << NSStringFromClass(suggestion.class)
        << " not handled for deletion.";
  }
}

- (void)onScroll {
  [self.omniboxAutocompleteController onScroll];
}

- (void)requestResultsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount {
  [self.omniboxAutocompleteController
      requestSuggestionsWithVisibleSuggestionCount:visibleSuggestionCount];
}

- (void)previewSuggestion:(id<AutocompleteSuggestion>)suggestion
            isFirstUpdate:(BOOL)isFirstUpdate {
  [self.omniboxAutocompleteController previewSuggestion:suggestion
                                          isFirstUpdate:isFirstUpdate];
}

#pragma mark OmniboxPopupMutator Private

/// Logs selected tile index and type.
- (void)logSelectedAutocompleteTile:(const AutocompleteMatch&)match {
  DCHECK(match.type == AutocompleteMatchType::TILE_NAVSUGGEST);
  for (size_t i = 0; i < match.suggest_tiles.size(); ++i) {
    const AutocompleteMatch::SuggestTile& tile = match.suggest_tiles[i];
    // AutocompleteMatch contains all tiles, find the tile corresponding to the
    // match. See how tiles are unwrapped in `extractMatches`.
    if (match.destination_url == tile.url) {
      // Log selected tile index. Note: When deleting a tile, the index may
      // shift, this is not taken into account.
      base::UmaHistogramExactLinear("Omnibox.SuggestTiles.SelectedTileIndex", i,
                                    kMaxSuggestTileTypePosition);
      int tileType =
          tile.is_search ? SuggestTileType::kSearch : SuggestTileType::kURL;
      base::UmaHistogramExactLinear("Omnibox.SuggestTiles.SelectedTileType",
                                    tileType, SuggestTileType::kCount);
      return;
    }
  }
}

#pragma mark - ImageRetriever

- (void)fetchImage:(GURL)imageURL completion:(void (^)(UIImage*))completion {
  [_omniboxImageFetcher fetchImage:imageURL completion:completion];
}

#pragma mark - FaviconRetriever

- (void)fetchFavicon:(GURL)pageURL completion:(void (^)(UIImage*))completion {
  [_omniboxImageFetcher fetchFavicon:pageURL completion:completion];
}

#pragma mark - Private methods

- (void)logPedalShownForCurrentResult {
  for (id<AutocompleteSuggestionGroup> group in _suggestionGroups) {
    if (group.type != SuggestionGroupType::kPedalSuggestionGroup) {
      continue;
    }

    for (id<AutocompleteSuggestion> pedalMatch in group.suggestions) {
      base::UmaHistogramEnumeration("Omnibox.PedalShown",
                                    (OmniboxPedalId)pedalMatch.pedal.type,
                                    OmniboxPedalId::TOTAL_COUNT);
    }
  }
}

/// Log action in suggest shown but not used for the current result.
- (void)logActionsInSuggestShownForCurrentResult {
  for (id<AutocompleteSuggestionGroup> group in _suggestionGroups) {
    for (id<AutocompleteSuggestion> suggestion in group.suggestions) {
      if (suggestion.actionsInSuggest.count == 0) {
        continue;
      }
      for (SuggestAction* action in suggestion.actionsInSuggest) {
        OmniboxActionInSuggest::RecordShownAndUsedMetrics(action.type,
                                                          false /* used */);
      }
    }
  }
}

- (void)callActionTapped {
  [self.omniboxAutocompleteController onCallAction];
}

#pragma mark - CarouselItemMenuProvider

/// Context Menu for carousel `item` in `view`.
- (UIContextMenuConfiguration*)
    contextMenuConfigurationForCarouselItem:(CarouselItem*)carouselItem
                                   fromView:(UIView*)view {
  __weak __typeof(self) weakSelf = self;
  __weak CarouselItem* weakItem = carouselItem;
  GURL copyURL = carouselItem.URL.gurl;

  UIContextMenuActionProvider actionProvider = ^(
      NSArray<UIMenuElement*>* suggestedActions) {
    DCHECK(weakSelf);

    __typeof(self) strongSelf = weakSelf;
    BrowserActionFactory* actionFactory = strongSelf.mostVisitedActionFactory;

    // Record that this context menu was shown to the user.
    RecordMenuShown(kMenuScenarioHistogramOmniboxMostVisitedEntry);

    NSMutableArray<UIMenuElement*>* menuElements =
        [[NSMutableArray alloc] init];

    [menuElements addObject:[actionFactory actionToOpenInNewTabWithURL:copyURL
                                                            completion:nil]];

    UIAction* incognitoAction =
        [actionFactory actionToOpenInNewIncognitoTabWithURL:copyURL
                                                 completion:nil];

    if (!self.allowIncognitoActions) {
      // Disable the "Open in Incognito" option if the incognito mode is
      // disabled.
      incognitoAction.attributes = UIMenuElementAttributesDisabled;
    }

    [menuElements addObject:incognitoAction];

    if (base::ios::IsMultipleScenesSupported()) {
      UIAction* newWindowAction = [actionFactory
          actionToOpenInNewWindowWithURL:copyURL
                          activityOrigin:
                              WindowActivityContentSuggestionsOrigin];
      [menuElements addObject:newWindowAction];
    }

    CrURL* URL = [[CrURL alloc] initWithGURL:copyURL];
    [menuElements addObject:[actionFactory actionToCopyURL:URL]];

    [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                    [weakSelf.sharingDelegate popupMediator:weakSelf
                                                   shareURL:copyURL
                                                      title:carouselItem.title
                                                 originView:view];
                  }]];

    [menuElements addObject:[actionFactory actionToRemoveWithBlock:^{
                    [weakSelf removeMostVisitedForURL:copyURL
                                     withCarouselItem:weakItem];
                  }]];

    return [UIMenu menuWithTitle:@"" children:menuElements];
  };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (NSArray<UIAccessibilityCustomAction*>*)
    accessibilityActionsForCarouselItem:(CarouselItem*)carouselItem
                               fromView:(UIView*)view {
  __weak __typeof(self) weakSelf = self;
  __weak CarouselItem* weakItem = carouselItem;
  __weak UIView* weakView = view;
  GURL copyURL = carouselItem.URL.gurl;

  NSMutableArray* actions = [[NSMutableArray alloc] init];

  {  // Open in new tab
    UIAccessibilityCustomActionHandler openInNewTabBlock =
        ^BOOL(UIAccessibilityCustomAction*) {
          [weakSelf openNewTabWithMostVisitedItem:weakItem incognito:NO];
          return YES;
        };
    UIAccessibilityCustomAction* openInNewTab =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(
                              IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
            actionHandler:openInNewTabBlock];
    [actions addObject:openInNewTab];
  }
  {  // Remove
    UIAccessibilityCustomActionHandler removeBlock =
        ^BOOL(UIAccessibilityCustomAction*) {
          [weakSelf removeMostVisitedForURL:copyURL withCarouselItem:weakItem];
          return YES;
        };
    UIAccessibilityCustomAction* removeMostVisited =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(
                              IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)
            actionHandler:removeBlock];
    [actions addObject:removeMostVisited];
  }
  if (self.allowIncognitoActions) {  // Open in new incognito tab
    UIAccessibilityCustomActionHandler openInNewIncognitoTabBlock =
        ^BOOL(UIAccessibilityCustomAction*) {
          [weakSelf openNewTabWithMostVisitedItem:weakItem incognito:YES];
          return YES;
        };
    UIAccessibilityCustomAction* openInIncognitoNewTab =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(
                              IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
            actionHandler:openInNewIncognitoTabBlock];
    [actions addObject:openInIncognitoNewTab];
  }
  if (base::ios::IsMultipleScenesSupported()) {  // Open in new window
    UIAccessibilityCustomActionHandler openInNewWindowBlock = ^BOOL(
        UIAccessibilityCustomAction*) {
      NSUserActivity* activity =
          ActivityToLoadURL(WindowActivityContentSuggestionsOrigin, copyURL);
      [weakSelf.applicationCommandsHandler openNewWindowWithActivity:activity];
      return YES;
    };
    UIAccessibilityCustomAction* newWindowAction =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(
                              IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW)
            actionHandler:openInNewWindowBlock];
    [actions addObject:newWindowAction];
  }
  {  // Copy
    UIAccessibilityCustomActionHandler copyBlock =
        ^BOOL(UIAccessibilityCustomAction*) {
          StoreURLInPasteboard(copyURL);
          return YES;
        };
    UIAccessibilityCustomAction* copyAction =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(IDS_IOS_COPY_LINK_ACTION_TITLE)
            actionHandler:copyBlock];
    [actions addObject:copyAction];
  }
  {  // Share
    UIAccessibilityCustomActionHandler shareBlock =
        ^BOOL(UIAccessibilityCustomAction*) {
          [weakSelf.sharingDelegate popupMediator:weakSelf
                                         shareURL:copyURL
                                            title:weakItem.title
                                       originView:weakView];
          return YES;
        };
    UIAccessibilityCustomAction* shareAction =
        [[UIAccessibilityCustomAction alloc]
             initWithName:l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL)
            actionHandler:shareBlock];
    [actions addObject:shareAction];
  }

  return actions;
}

#pragma mark CarouselItemMenuProvider Private

/// Blocks `URL` so it won't appear in most visited URLs.
- (void)blockMostVisitedURL:(GURL)URL {
  scoped_refptr<history::TopSites> top_sites = [self.protocolProvider topSites];
  if (top_sites) {
    top_sites->AddBlockedUrl(URL);
  }
}

/// Blocks `URL` in most visited sites and hides `CarouselItem` if it still
/// exist.
- (void)removeMostVisitedForURL:(GURL)URL
               withCarouselItem:(CarouselItem*)carouselItem {
  if (!carouselItem) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("MostVisited_UrlBlocklisted_Omnibox"));
  [self blockMostVisitedURL:URL];
  [self.carouselItemConsumer deleteCarouselItem:carouselItem];
}

/// Opens `carouselItem` in a new tab.
/// `incognito`: open in incognito tab.
- (void)openNewTabWithMostVisitedItem:(CarouselItem*)carouselItem
                            incognito:(BOOL)incognito {
  DCHECK(self.applicationCommandsHandler);
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:carouselItem.URL.gurl
                                      inIncognito:incognito];
  [self.applicationCommandsHandler openURLInNewTab:command];
}

/// Opens suggestAction in a new tab.
- (void)openNewTabWithSuggestAction:(SuggestAction*)suggestAction {
  DCHECK(self.applicationCommandsHandler);
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:suggestAction.actionURI
                                      inIncognito:NO];
  [self.applicationCommandsHandler openURLInNewTab:command];
}

@end
