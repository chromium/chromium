// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"

#include "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#include "components/ntp_snippets/content_suggestions_metrics.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/suggested_content.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_category_wrapper.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Values for the UMA ContentSuggestions.Feed.EngagementType
// histogram. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused. This must be kept
// in sync with FeedEngagementType in enums.xml.
enum class FeedEngagementType {
  kFeedEngaged = 0,
  kFeedEngagedSimple = 1,
  kFeedInteracted = 2,
  kDeprecatedFeedScrolled = 3,
  kFeedScrolled = 4,
  kMaxValue = kFeedScrolled,
};

namespace {
// Histogram name for the feed engagement types.
const char kDiscoverFeedEngagementTypeHistogram[] =
    "ContentSuggestions.Feed.EngagementType";

// Minimum scrolling amount to record a FeedEngagementType::kFeedEngaged due to
// scrolling.
const int kMinScrollThreshold = 160;

// Time between two metrics recorded to consider it a new session.
const int kMinutesBetweenSessions = 5;
}

@interface ContentSuggestionsMetricsRecorder ()

// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedEngagedSimple.
@property(nonatomic, assign) BOOL engagedSimpleReported;
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedEngaged.
@property(nonatomic, assign) BOOL engagedReported;
// Tracking property to avoid duplicate recordings of
// FeedEngagementType::kFeedScrolled.
@property(nonatomic, assign) BOOL scrolledReported;
// The time when the first metric is being recorded for this session.
@property(nonatomic, assign) base::Time sessionStartTime;

@end

@implementation ContentSuggestionsMetricsRecorder

@synthesize delegate = _delegate;

#pragma mark - Public

- (void)onSuggestionOpened:(ContentSuggestionsItem*)item
               atIndexPath:(NSIndexPath*)indexPath
        sectionsShownAbove:(NSInteger)sectionsShownAbove
     suggestionsShownAbove:(NSInteger)suggestionsAbove
                withAction:(WindowOpenDisposition)action {
  ContentSuggestionsSectionInformation* sectionInfo =
      item.suggestionIdentifier.sectionInfo;
  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [self.delegate categoryWrapperForSectionInfo:sectionInfo];

  ntp_snippets::metrics::OnSuggestionOpened(
      suggestionsAbove + indexPath.item, [categoryWrapper category],
      sectionsShownAbove, indexPath.item, item.publishDate, item.score, action,
      /*is_prefetched=*/false, /*is_offline=*/false);
  [self recordInteraction];
}

- (void)onMenuOpenedForSuggestion:(ContentSuggestionsItem*)item
                      atIndexPath:(NSIndexPath*)indexPath
            suggestionsShownAbove:(NSInteger)suggestionsAbove {
  ContentSuggestionsSectionInformation* sectionInfo =
      item.suggestionIdentifier.sectionInfo;
  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [self.delegate categoryWrapperForSectionInfo:sectionInfo];

  ntp_snippets::metrics::OnSuggestionMenuOpened(
      suggestionsAbove + indexPath.item, [categoryWrapper category],
      indexPath.item, item.publishDate, item.score);
  [self recordInteraction];
}

#pragma mark - ContentSuggestionsMetricsRecording

- (void)onSuggestionShown:(CollectionViewItem*)item
              atIndexPath:(NSIndexPath*)indexPath
    suggestionsShownAbove:(NSInteger)suggestionsAbove {
  ContentSuggestionsItem* suggestion =
      base::mac::ObjCCastStrict<ContentSuggestionsItem>(item);
  ContentSuggestionsSectionInformation* sectionInfo =
      suggestion.suggestionIdentifier.sectionInfo;
  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [self.delegate categoryWrapperForSectionInfo:sectionInfo];

  ntp_snippets::metrics::OnSuggestionShown(
      suggestionsAbove + indexPath.item, [categoryWrapper category],
      indexPath.item, suggestion.publishDate, suggestion.score,
      suggestion.fetchDate);
}

- (void)onMoreButtonTappedAtPosition:(NSInteger)position
                           inSection:(ContentSuggestionsSectionInformation*)
                                         sectionInfo {
  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [self.delegate categoryWrapperForSectionInfo:sectionInfo];

  ntp_snippets::metrics::OnMoreButtonClicked([categoryWrapper category],
                                             position);
  [self recordInteraction];
}

- (void)onSuggestionDismissed:(CollectionViewItem<SuggestedContent>*)item
                  atIndexPath:(NSIndexPath*)indexPath
        suggestionsShownAbove:(NSInteger)suggestionsAbove {
  ContentSuggestionsSectionInformation* sectionInfo =
      item.suggestionIdentifier.sectionInfo;
  ContentSuggestionsCategoryWrapper* categoryWrapper =
      [self.delegate categoryWrapperForSectionInfo:sectionInfo];

  ntp_snippets::metrics::OnSuggestionDismissed(
      suggestionsAbove + indexPath.item, [categoryWrapper category],
      indexPath.item, /*visited=*/false);
  [self recordInteraction];
}

- (void)recordFeedScrolled:(int)scrollDistance {
  DCHECK(!IsDiscoverFeedEnabled());
  [self recordEngagement:scrollDistance interacted:NO];

  if (!self.scrolledReported) {
    [self recordEngagementTypeHistogram:FeedEngagementType::kFeedScrolled];
    self.scrolledReported = YES;
  }
}

#pragma mark - Private

// Records Feed engagement.
- (void)recordEngagement:(int)scrollDistance interacted:(BOOL)interacted {
  scrollDistance = abs(scrollDistance);

  // Determine if this interaction is part of a new 'session'.
  base::Time now = base::Time::Now();
  base::TimeDelta visitTimeout =
      base::TimeDelta::FromMinutes(kMinutesBetweenSessions);
  if (now - self.sessionStartTime > visitTimeout) {
    [self finalizeSession];
  }
  // Reset the last active time for session measurement.
  self.sessionStartTime = now;

  // Report the user as engaged-simple if they have scrolled any amount or
  // interacted with the card, and we have not already reported it for this
  // chrome run.
  if (!self.engagedSimpleReported && (scrollDistance > 0 || interacted)) {
    [self recordEngagementTypeHistogram:FeedEngagementType::kFeedEngagedSimple];
    self.engagedSimpleReported = YES;
  }

  // Report the user as engaged if they have scrolled more than the threshold or
  // interacted with the card, and we have not already reported it this chrome
  // run.
  if (!self.engagedReported &&
      (scrollDistance > kMinScrollThreshold || interacted)) {
    [self recordEngagementTypeHistogram:FeedEngagementType::kFeedEngaged];
    self.engagedReported = YES;
  }
}

// Records Engagement histograms of |engagementType|.
- (void)recordEngagementTypeHistogram:(FeedEngagementType)engagementType {
  UMA_HISTOGRAM_ENUMERATION(kDiscoverFeedEngagementTypeHistogram,
                            engagementType);
}

// Records any direct interaction with the Feed, this doesn't include scrolling.
- (void)recordInteraction {
  DCHECK(!IsDiscoverFeedEnabled());
  [self recordEngagement:0 interacted:YES];
  [self recordEngagementTypeHistogram:FeedEngagementType::kFeedInteracted];
}

// Resets the session tracking values, this occurs if there's been
// kMinutesBetweenSessions minutes between sessions.
- (void)finalizeSession {
  if (!self.engagedSimpleReported)
    return;
  self.engagedReported = NO;
  self.engagedSimpleReported = NO;
  self.scrolledReported = NO;
}

@end
