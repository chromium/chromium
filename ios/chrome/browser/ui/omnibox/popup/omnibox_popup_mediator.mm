// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/common/omnibox_features.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/favicon/favicon_attributes.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kOmniboxIconSize = 16;
}  // namespace

@implementation OmniboxPopupMediator {
  // Fetcher for Answers in Suggest images.
  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> _imageFetcher;

  OmniboxPopupMediatorDelegate* _delegate;  // weak

  AutocompleteResult _currentResult;
}
@synthesize consumer = _consumer;
@synthesize hasResults = _hasResults;
@synthesize incognito = _incognito;
@synthesize open = _open;
@synthesize presenter = _presenter;

- (instancetype)initWithFetcher:
                    (std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper>)
                        imageFetcher
                  faviconLoader:(FaviconLoader*)faviconLoader
                       delegate:(OmniboxPopupMediatorDelegate*)delegate {
  self = [super init];
  if (self) {
    DCHECK(delegate);
    _delegate = delegate;
    _imageFetcher = std::move(imageFetcher);
    _faviconLoader = faviconLoader;
    _open = NO;
  }
  return self;
}

- (void)updateMatches:(const AutocompleteResult&)result
        withAnimation:(BOOL)animation {
  _currentResult.Reset();
  _currentResult.CopyFrom(result);

  self.hasResults = !_currentResult.empty();

  [self.consumer updateMatches:[self wrappedMatches] withAnimation:animation];
}

- (NSArray<id<AutocompleteSuggestion>>*)wrappedMatches {
  NSMutableArray<id<AutocompleteSuggestion>>* wrappedMatches =
      [[NSMutableArray alloc] init];

  size_t size = _currentResult.size();
  for (size_t i = 0; i < size; i++) {
    const AutocompleteMatch& match =
        ((const AutocompleteResult&)_currentResult).match_at((NSUInteger)i);
    AutocompleteMatchFormatter* formatter =
        [AutocompleteMatchFormatter formatterWithMatch:match];
    formatter.starred = _delegate->IsStarredMatch(match);
    formatter.incognito = _incognito;
    formatter.defaultSearchEngineIsGoogle = self.defaultSearchEngineIsGoogle;
    [wrappedMatches addObject:formatter];
  }

  return wrappedMatches;
}

- (void)updateWithResults:(const AutocompleteResult&)result {
  if (!self.open && !result.empty()) {
    // The popup is not currently open and there are results to display. Update
    // and animate the cells
    [self updateMatches:result withAnimation:YES];
  } else {
    // The popup is already displayed or there are no results to display. Update
    // the cells without animating.
    [self updateMatches:result withAnimation:NO];
  }
  self.open = !result.empty();

  [self.presenter updatePopup];
}

- (void)setTextAlignment:(NSTextAlignment)alignment {
  [self.consumer setTextAlignment:alignment];
}

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  [self.consumer setSemanticContentAttribute:semanticContentAttribute];
}

#pragma mark - AutocompleteResultConsumerDelegate

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                   didHighlightRow:(NSUInteger)row {
  _delegate->OnMatchHighlighted(row);
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
                      didSelectRow:(NSUInteger)row {
  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(row);

  _delegate->OnMatchSelected(match, row, WindowOpenDisposition::CURRENT_TAB);
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
        didTapTrailingButtonForRow:(NSUInteger)row {
  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(row);

  if (match.has_tab_match) {
    _delegate->OnMatchSelected(match, row,
                               WindowOpenDisposition::SWITCH_TO_TAB);
  } else {
    if (AutocompleteMatch::IsSearchType(match.type)) {
      base::RecordAction(
          base::UserMetricsAction("MobileOmniboxRefineSuggestion.Search"));
    } else {
      base::RecordAction(
          base::UserMetricsAction("MobileOmniboxRefineSuggestion.Url"));
    }
    _delegate->OnMatchSelectedForAppending(match);
  }
}

- (void)autocompleteResultConsumer:(id<AutocompleteResultConsumer>)sender
           didSelectRowForDeletion:(NSUInteger)row {
  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(row);
  _delegate->OnMatchSelectedForDeletion(match);
}

- (void)autocompleteResultConsumerDidScroll:
    (id<AutocompleteResultConsumer>)sender {
  _delegate->OnScroll();
}

#pragma mark - ImageFetcher

- (void)fetchImage:(GURL)imageURL completion:(void (^)(UIImage*))completion {
  image_fetcher::ImageDataFetcherBlock callback =
      ^(NSData* data, const image_fetcher::RequestMetadata& metadata) {
        if (data) {
          UIImage* image =
              [UIImage imageWithData:data scale:[UIScreen mainScreen].scale];
          completion(image);
        } else {
          completion(nil);
        }
      };
  _imageFetcher->FetchImageDataWebpDecoded(imageURL, callback);
}

#pragma mark - FaviconRetriever

- (void)fetchFavicon:(GURL)pageURL completion:(void (^)(UIImage*))completion {
  if (!self.faviconLoader) {
    return;
  }

  self.faviconLoader->FaviconForPageUrl(
      pageURL, kOmniboxIconSize, kOmniboxIconSize,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        if (attributes.faviconImage && !attributes.usesDefaultImage)
          completion(attributes.faviconImage);
      });
}

@end
