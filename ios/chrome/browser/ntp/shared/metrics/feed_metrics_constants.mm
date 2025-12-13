// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"

const int kMinScrollThreshold = 140;
const int kGoodVisitTimeInFeedSeconds = 60;
const int kNonShortClickSeconds = 10;
const int kMinutesBetweenSessions = 5;
const int kMaxCardsInFeed = 50;

const char kArticleVisitTimestampKey[] = "ShortClickInteractionTimestamp";
const char kLongDiscoverFeedVisitTimeAggregateKey[] =
    "LongDiscoverFeedInteractionTimeDelta";
const char kLastInteractionTimeForDiscoverGoodVisits[] =
    "LastInteractionTimeForGoodVisitsDiscover";
const char kLastDayTimeInFeedReportedKey[] = "LastDayTimeInFeedReported";
const char kTimeSpentInFeedAggregateKey[] = "TimeSpentInFeedAggregate";
const char kActivityBucketLastReportedDateKey[] =
    "ActivityBucketLastReportedDate";
const char kActivityBucketLastReportedDateArrayKey[] =
    "ActivityBucketLastReportedDateArray";

#pragma mark - Histograms

const char kTimeSpentInFeedHistogram[] =
    "ContentSuggestions.Feed.TimeSpentInFeed";
const char kDiscoverFeedUserActionHistogram[] =
    "ContentSuggestions.Feed.UserActions";
const char kDiscoverFeedUserActionCommandHistogram[] =
    "ContentSuggestions.Feed.UserActions.Commands";
const char kDiscoverFeedEngagementTypeHistogram[] =
    "ContentSuggestions.Feed.EngagementType";
const char kDiscoverFeedCardShownAtIndex[] =
    "NewTabPage.ContentSuggestions.Shown";
const char kAllFeedsActivityBucketsHistogram[] =
    "ContentSuggestions.Feed.AllFeeds.Activity";
const char kDiscoverFeedNoticeCardFulfilled[] =
    "ContentSuggestions.Feed.NoticeCardFulfilled2";
const char kDiscoverFeedArticlesFetchNetworkDurationSuccess[] =
    "ContentSuggestions.Feed.Network.Duration.ArticlesFetchSuccess";
const char kDiscoverFeedArticlesFetchNetworkDurationFailure[] =
    "ContentSuggestions.Feed.Network.Duration.ArticlesFetchFailure";
const char kDiscoverFeedMoreArticlesFetchNetworkDurationSuccess[] =
    "ContentSuggestions.Feed.Network.Duration.MoreArticlesFetchSuccess";
const char kDiscoverFeedMoreArticlesFetchNetworkDurationFailure[] =
    "ContentSuggestions.Feed.Network.Duration.MoreArticlesFetchFailure";
const char kDiscoverFeedUploadActionsNetworkDurationSuccess[] =
    "ContentSuggestions.Feed.Network.Duration.ActionUploadSuccess";
const char kDiscoverFeedUploadActionsNetworkDurationFailure[] =
    "ContentSuggestions.Feed.Network.Duration.ActionUploadFailure";
const char kDiscoverFeedNetworkDuration[] =
    "ContentSuggestions.Feed.Network.Duration";
const char kDiscoverFeedURLOpened[] = "NewTabPage.ContentSuggestions.Opened";
const char kDiscoverFeedActivityLoggingEnabled[] =
    "ContentSuggestions.Feed.ActivityLoggingEnabled";
const char kDiscoverFeedBrokenNTPHierarchy[] =
    "ContentSuggestions.Feed.BrokenNTPHierarchy";
const char kDiscoverFeedRefreshTrigger[] =
    "ContentSuggestions.Feed.RefreshTrigger";
const char kDiscoverUniformityFlag[] = "ContentSuggestions.Feed.UniformityFlag";
const char kFeedUserSettingsOnStart[] =
    "ContentSuggestions.Feed.UserSettingsOnStart";
const char kFeedSignInUI[] = "ContentSuggestions.Feed.FeedSignInUI";
const char kFeedSyncPromo[] = "ContentSuggestions.Feed.FeedSyncPromo";
const char kFeedHandlingErrorPrefix[] = "IOS.FeedHandlingError.";

#pragma mark - User Actions

const char kDiscoverFeedHistogramDeviceOrientationChangedToPortrait[] =
    "ContentSuggestions.Feed.DeviceOrientationChanged.Portrait";
const char kDiscoverFeedHistogramDeviceOrientationChangedToLandscape[] =
    "ContentSuggestions.Feed.DeviceOrientationChanged.Landscape";
const char kDiscoverFeedUserActionOpenSameTab[] =
    "ContentSuggestions.Feed.CardAction.Open";
const char kDiscoverFeedUserActionOpenIncognitoTab[] =
    "ContentSuggestions.Feed.CardAction.OpenInNewIncognitoTab";
const char kDiscoverFeedUserActionOpenNewTab[] =
    "ContentSuggestions.Feed.CardAction.OpenInNewTab";
const char kDiscoverFeedUserActionReadLaterTapped[] =
    "ContentSuggestions.Feed.CardAction.ReadLater";
const char kDiscoverFeedUserActionSendFeedbackOpened[] =
    "ContentSuggestions.Feed.CardAction.SendFeedback";
const char kDiscoverFeedUserActionContextMenuOpened[] =
    "ContentSuggestions.Feed.CardAction.ContextMenu";
const char kDiscoverFeedUserActionHideStory[] =
    "ContentSuggestions.Feed.CardAction.HideStory";
const char kDiscoverFeedUserActionCloseContextMenu[] =
    "ContentSuggestions.Feed.CardAction.ClosedContextMenu";
const char kDiscoverFeedUserActionNativeActionSheetOpened[] =
    "ContentSuggestions.Feed.CardAction.OpenNativeActionSheet";
const char kDiscoverFeedUserActionNativeContextMenuOpened[] =
    "ContentSuggestions.Feed.CardAction.OpenNativeContextMenu";
const char kDiscoverFeedUserActionNativeContextMenuClosed[] =
    "ContentSuggestions.Feed.CardAction.CloseNativeContextMenu";
const char kDiscoverFeedUserActionNativePulldownMenuOpened[] =
    "ContentSuggestions.Feed.CardAction.OpenNativePulldownMenu";
const char kDiscoverFeedUserActionNativePulldownMenuClosed[] =
    "ContentSuggestions.Feed.CardAction.CloseNativePulldownMenu";
const char kDiscoverFeedUserActionReportContentOpened[] =
    "ContentSuggestions.Feed.CardAction.ReportContent";
const char kDiscoverFeedUserActionReportContentClosed[] =
    "ContentSuggestions.Feed.CardAction.ClosedReportContent";
const char kDiscoverFeedUserActionPreviewTapped[] =
    "ContentSuggestions.Feed.CardAction.TapPreview";
const char kDiscoverFeedUserActionEngaged[] = "ContentSuggestions.Feed.Engaged";
const char kFeedWillRefresh[] = "ContentSuggestions.Feed.WillRefresh";
const char kNTPViewHierarchyFixed[] = "NewTabPage.ViewHierarchyFixed";
const char kShowFeedSignInOnlyUIWithUserId[] =
    "ContentSuggestions.Feed.SignIn.ShowFeedSignInOnlyUIWithUserId";
const char kShowFeedSignInOnlyUIWithoutUserId[] =
    "ContentSuggestions.Feed.SignIn.ShowFeedSignInOnlyUIWithoutUserId";
const char kShowSignInOnlyFlowFromFeed[] =
    "ContentSuggestions.Feed.SignIn.ShowSignInOnlyFlowFromFeed";
const char kShowSignInDisableToastFromFeed[] =
    "ContentSuggestions.Feed.SignIn.ShowSignInDisableToastFromFeed";
const char kShowSyncFlowFromFeed[] =
    "ContentSuggestions.Feed.Sync.ShowSyncFlowFromFeed";
const char kShowDisableToastFromFeed[] =
    "ContentSuggestions.Feed.Sync.ShowDisableToastFromFeed";
