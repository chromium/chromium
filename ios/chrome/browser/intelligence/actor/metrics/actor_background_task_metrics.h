// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_METRICS_ACTOR_BACKGROUND_TASK_METRICS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_METRICS_ACTOR_BACKGROUND_TASK_METRICS_H_

namespace actor {

// Records how an actor's background continued processing task ended.
inline constexpr char kActorBackgroundTaskEndReasonHistogram[] =
    "IOS.Actor.BackgroundTask.EndReason";

// Records how long an actor's background continued processing task lived,
// from the time the actor received its context until it ended.
inline constexpr char kActorBackgroundTaskDurationHistogram[] =
    "IOS.Actor.BackgroundTask.Duration";

// Records whether an active actor task was eligible to request a background
// continued processing task when the app entered the background.
inline constexpr char kActorBackgroundTaskRequestEligibilityHistogram[] =
    "IOS.Actor.BackgroundTask.RequestEligibility";

// Records why an active actor task was not eligible to request a background
// continued processing task when the app entered the background.
inline constexpr char kActorBackgroundTaskIneligibilityReasonHistogram[] =
    "IOS.Actor.BackgroundTask.IneligibilityReason";

// Why an active actor task was not eligible to request a background continued
// processing task when the app entered the background. Logged to UMA, so
// entries must not be renumbered or reused.
// LINT.IfChange(IOSActorBackgroundTaskIneligibilityReason)
enum class ActorBackgroundTaskIneligibilityReason {
  // Actor backgrounding is disabled.
  kBackgroundingDisabled = 0,
  // The worker already holds a live background task context.
  kAlreadyHasContext = 1,
  // The task is in a state that does not keep a background task alive (waiting
  // on the user, paused, or terminal).
  kStateDoesNotKeepAlive = 2,
  kMaxValue = kStateDoesNotKeepAlive,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSActorBackgroundTaskIneligibilityReason)

// Why an actor's background continued processing task ended. Logged to UMA,
// so entries must not be renumbered or reused.
// LINT.IfChange(IOSActorBackgroundTaskEndReason)
enum class ActorBackgroundTaskEndReason {
  // The actor task was stopped with success.
  kSucceeded = 0,
  // The actor task was stopped internally with failure.
  kFailed = 1,
  // The actor task entered a state that waits on the user (e.g., yielded or
  // paused).
  kWaitingOnUser = 2,
  // The background task was expired by the system.
  kExpired = 3,
  // The user cancelled the background task from the system-provided Live
  // Activity.
  kCancelled = 4,
  // The actor task was destroyed while holding a live context.
  kDestroyed = 5,
  kMaxValue = kDestroyed,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSActorBackgroundTaskEndReason)

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_METRICS_ACTOR_BACKGROUND_TASK_METRICS_H_
