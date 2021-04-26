// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_SERVICE_THROTTLER_H_
#define MEDIA_BASE_ANDROID_MEDIA_SERVICE_THROTTLER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {
class MediaServerCrashListener;

// The MediaServiceThrottler's purpose is to prevent a compromised process from
// attempting to crash the MediaServer, by repeatedly requesting resources or
// issuing malformed requests. It is used to delay the creation of Android
// MediaServer clients (currently only the MediaPlayerBridge) by some amount
// that makes it impractical to DOS the MediaServer (by requesting the
// playback of hundreds of malformed URLs per second, for example).
//
// GetDelayForClientCreation() linearly spaces out client creations and
// guarantees that the clients will never be scheduled faster than some
// threshold (see the .cc file for the latest values).
// The MediaServiceThrottler also uses a MediaServerCrashListener to monitor for
// MediaServer crashes. The delay between client creations is exponentially
// increased (up to a cap) based on the number of recent MediaServer crashes.
//
// NOTE: The MediaServiceThrottler has small moving window that allows a certain
// number of clients to be immediately scheduled, while still respecting the
// max scheduling rates. This allows clients to be 'burst created' to account
// for a burst of requests from a new page load.
//
// For an example of usage, look at MediaPlayerRenderer::Initialize().
class MEDIA_EXPORT MediaServiceThrottler {
 public:
  // Called to get the singleton MediaServiceThrottler instance.
  // The first thread on which GetInstance() is called is the thread on which
  // calls to OnMediaServerCrash() will be signaled.
  static MediaServiceThrottler* GetInstance();

  // Returns the delay to wait until a new client is allowed to be created.
  base::TimeDelta GetDelayForClientCreation();

  // Test only methods.
  void SetTickClockForTesting(const base::TickClock* clock);
  void ResetInternalStateForTesting();
  base::TimeDelta GetBaseThrottlingRateForTesting();
  bool IsCrashListenerAliveForTesting();
  void SetCrashListenerTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> crash_listener_task_runner);

 private:
  friend class MediaServiceThrottlerTest;

  MediaServiceThrottler();
  virtual ~MediaServiceThrottler();

  // Called by the |crash_listener_| whenever a crash is detected.
  void OnMediaServerCrash(bool watchdog_needs_release);

  // Updates |current_crashes_| according to a linear decay function.
  void UpdateServerCrashes();

  // Ensures that the MediaServerCrashListener was properly started (can lead
  // to OnMediaServerCrash() being called in the case it hasn't).
  void EnsureCrashListenerStarted();

  // Frees up the resources used by |crash_listener_|;
  void ReleaseCrashListener();

  // Gets the delay for ScheduleClientCreation(), which grows exponentially
  // based on |current_crashes_|.
  base::TimeDelta GetThrottlingDelayFromServerCrashes();

  const base::TickClock* clock_;

  // Effective number of media server crashes.
  // NOTE: This is of type double because we decay the number of crashes at a
  // rate of one per minute (e.g. 30s after a single crash, |current_crashes_|
  // should be equal to 0.5).
  double current_crashes_;

  // Next time at which a client creation can be scheduled.
  base::TimeTicks next_schedulable_slot_;

  // Last media server crash time.
  base::TimeTicks last_server_crash_;

  // Last time UpdateServerCrashes() was called.
  base::TimeTicks last_current_crash_update_time_;

  // Last time ScheduleClientCreation() was called.
  base::TimeTicks last_schedule_call_;

  // Callbacks used to release |crash_listener_| after 60s of inactivity.
  base::RepeatingClosure release_crash_listener_cb_;
  base::CancelableRepeatingClosure cancelable_release_crash_listener_cb_;

  // Listens for MediaServer crashes using a watchdog MediaPlayer.
  std::unique_ptr<MediaServerCrashListener> crash_listener_;

  scoped_refptr<base::SingleThreadTaskRunner> crash_listener_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MediaServiceThrottler);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_SERVICE_THROTTLER_H_
