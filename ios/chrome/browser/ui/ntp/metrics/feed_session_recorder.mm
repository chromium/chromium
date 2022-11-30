// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/metrics/feed_session_recorder.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_session_recorder+testing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Histogram name to measure the duration of a feed session. Note that
// `kSessionTimeout` directly affects how this is measured.
const char kDiscoverFeedSessionDuration[] =
    "ContentSuggestions.Feed.SessionDuration";

// Histogram name to measure the time between user interactions (including
// scrolling) in the feed. This can inform how we set `kSessionTimeout`.
const char kDiscoverFeedTimeBetweenInteractions[] =
    "ContentSuggestions.Feed.TimeBetweenInteractions";

// Histogram name to measure the time between user sessions in the feed. Note
// that `kSessionTimeout` directly affects how this is measured.
const char kDiscoverFeedTimeBetweenSessions[] =
    "ContentSuggestions.Feed.TimeBetweenSessions";

// Key used to store the date (and time) of the user's previous interaction
// (including scrolling) with the feed. Can be nil if this is a fresh install or
// if there was an issue writing to NSUserDefaults.
NSString* const kFeedPreviousInteractionDateKey =
    @"DiscoverFeedPreviousInteractionDate";

// Two interactions (including scrolling) are considered to be within the same
// session if they are at most `kSessionTimeout` apart from each other.
// Otherwise, the later interaction (including scrolling) is considered a new
// session.
constexpr base::TimeDelta kSessionTimeout = base::Minutes(5);

}  // namespace

@interface FeedSessionRecorder ()

// The date (and time) when the first interaction (including scrolling) was
// recorded for the current session.
@property(nonatomic, assign) base::Time sessionStartDate;

// The date (and time) of the previous interaction (including scrolling).
@property(nonatomic, assign) base::Time previousInteractionDate;

// Session duration.
@property(nonatomic, assign) base::TimeDelta sessionDuration;

// Time between sessions.
@property(nonatomic, assign) base::TimeDelta timeBetweenSessions;

// Time between interactions.
@property(nonatomic, assign) base::TimeDelta timeBetweenInteractions;

@end

@implementation FeedSessionRecorder
@synthesize previousInteractionDate = _previousInteractionDate;

#pragma mark - Properties

- (base::Time)previousInteractionDate {
  if (_previousInteractionDate.is_null()) {
    // Reads from disk if there was a cold start and the property is null.
    // Disk value can be nil if this is a fresh install or if there was an issue
    // writing to NSUserDefaults.
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    NSDate* previousInteractionDate = base::mac::ObjCCast<NSDate>(
        [defaults objectForKey:kFeedPreviousInteractionDateKey]);
    if (previousInteractionDate) {
      _previousInteractionDate =
          base::Time::FromNSDate(previousInteractionDate);
    }
  }
  return _previousInteractionDate;
}

- (void)setPreviousInteractionDate:(base::Time)previousInteractionDate {
  // Sets both in-memory and disk memory at the same time.
  _previousInteractionDate = previousInteractionDate;
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:_previousInteractionDate.ToNSDate()
               forKey:kFeedPreviousInteractionDateKey];
  [defaults synchronize];
}

#pragma mark - Public

- (void)recordUserInteractionOrScrolling {
  [self recordUserInteractionOrScrollingAtDate:base::Time::Now()];
}

#pragma mark - Helpers

- (void)recordUserInteractionOrScrollingAtDate:(base::Time)interactionDate {
  if (self.previousInteractionDate.is_null()) {
    // This probably means this is the first feed interaction since a new
    // install. We cannot record session time metrics without this value so
    // start a new session.
    self.sessionStartDate = interactionDate;
  } else {
    [self
        recordTimeBetweenInteractionsWithStartDate:self.previousInteractionDate
                                           endDate:interactionDate];
    // Determine if this interaction should start a new session.
    const base::TimeDelta previousInteractionAge =
        interactionDate - self.previousInteractionDate;
    if (previousInteractionAge > kSessionTimeout) {
      // Close out current session.
      [self recordSessionDurationWithStartDate:self.sessionStartDate
                                       endDate:self.previousInteractionDate];
      [self recordTimeBetweenSessionsWithStartDate:self.previousInteractionDate
                                           endDate:interactionDate];
      // Start a new session.
      self.sessionStartDate = interactionDate;
    }
  }
  // Reset the previous active time for session measurement.
  self.previousInteractionDate = interactionDate;
}

// Records the session duration histogram. This tests our assumption that
// session durations are only a few seconds long. Our goal is to increase this
// over time. The histogram max value is 5 minutes. Session durations of 0
// indicate that there was only a single interaction or scrolling.
- (void)recordSessionDurationWithStartDate:(base::Time)startDate
                                   endDate:(base::Time)endDate {
  self.sessionDuration = endDate - startDate;
  UMA_HISTOGRAM_CUSTOM_TIMES(kDiscoverFeedSessionDuration, self.sessionDuration,
                             /*min=*/base::Milliseconds(0),
                             /*max=*/base::Minutes(5), /*bucket_count=*/100);
}

// Records the time between sessions histogram (i.e., how long it takes for
// users to return to a feed session). This can help us gauge how useful
// prefetching can be. The time between sessions is by definition longer than 5
// minutes. This histogram only captures those users who return to the feed
// within a day. The histogram max is 24 hours.
- (void)recordTimeBetweenSessionsWithStartDate:(base::Time)startDate
                                       endDate:(base::Time)endDate {
  self.timeBetweenSessions = endDate - startDate;
  UMA_HISTOGRAM_CUSTOM_TIMES(kDiscoverFeedTimeBetweenSessions,
                             self.timeBetweenSessions,
                             /*min=*/base::Minutes(5),
                             /*max=*/base::Hours(24), /*bucket_count=*/50);
}

// Records the time between interactions (including scrolling) histogram. We
// expect interactions within a single session to be within a few seconds to a
// minute or two of each other. We want to test if 5 minutes is the right
// session timeout value. The histogram max value is 10 minutes.
- (void)recordTimeBetweenInteractionsWithStartDate:(base::Time)startDate
                                           endDate:(base::Time)endDate {
  self.timeBetweenInteractions = endDate - startDate;
  UMA_HISTOGRAM_CUSTOM_TIMES(kDiscoverFeedTimeBetweenInteractions,
                             self.timeBetweenInteractions,
                             /*min=*/base::Milliseconds(1),
                             /*max=*/base::Minutes(10), /*bucket_count=*/50);
}

@end
