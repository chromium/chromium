// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_SHARED_METRICS_ASSISTANT_AIM_METRICS_RECORDER_H_
#define IOS_CHROME_BROWSER_COBROWSE_SHARED_METRICS_ASSISTANT_AIM_METRICS_RECORDER_H_

#import <Foundation/Foundation.h>

enum class AssistantAimEntrypoint;
enum class AssistantAimFRENoticeAction;
enum class AssistantAimHeaderAction;
enum class AssistantContainerDetent : NSInteger;

// Metrics recorder for user activity and session lifecycle during an Assistant
// AIM bottom sheet session.
@interface AssistantAIMMetricsRecorder : NSObject

// Starts tracking a session launched from `entrypoint`.
- (void)recordSessionStartedWithEntrypoint:(AssistantAimEntrypoint)entrypoint;

// Pauses or resumes the active visible duration timer when the sheet's
// `visible` state changes.
- (void)recordVisibilityChanged:(BOOL)visible;

// Records a sheet transition to `detent`.
- (void)recordDetentChanged:(AssistantContainerDetent)detent;

// Records whether the snackbar to undo closing the session resolved with
// `didUndo`
- (void)recordClosedSnackbarResolvedWithUndo:(BOOL)didUndo;

// Records a header button or overflow menu `action`.
- (void)recordHeaderAction:(AssistantAimHeaderAction)action;

// Finalizes and flushes all session-level metrics when the session ends.
- (void)recordSessionEnded;

// Records an outbound link click from the Assistant AIM.
- (void)recordLinkClicked;

// Records a follow-up query submission in the Assistant AIM bottom sheet with
// `attachmentCount` attached items.
- (void)recordFollowupSubmittedWithAttachmentCount:(NSUInteger)attachmentCount;

// Records that the user explicitly removed an auto-added tab attachment.
- (void)recordAutoAddedTabRemoved;

// Records the user `action` on the first run experience (FRE) notice alert.
- (void)recordFRENoticeAction:(AssistantAimFRENoticeAction)action;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_SHARED_METRICS_ASSISTANT_AIM_METRICS_RECORDER_H_
