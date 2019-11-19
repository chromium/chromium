// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_UMA_HELPER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_UMA_HELPER_H_

#include <memory>

#include "base/time/clock.h"
#include "content/common/content_export.h"

namespace base {
class TickClock;
}  // base namespace

namespace content {

class CONTENT_EXPORT MediaSessionUmaHelper {
 public:
  // This is used for UMA histogram (Media.Session.Suspended). New values should
  // be appended only and must be added before |Count|.
  enum class MediaSessionSuspendedSource {
    SystemTransient = 0,
    SystemPermanent = 1,
    UI = 2,
    CONTENT = 3,
    SystemTransientDuck = 4,
    kMaxValue = SystemTransientDuck,
  };

  // Extended enum to media_session::mojom::MediaSessionAction, distinguishing
  // default action handling.
  enum class MediaSessionUserAction {
    Play = 0,
    PlayDefault = 1,
    Pause = 2,
    PauseDefault = 3,
    StopDefault = 4,
    PreviousTrack = 5,
    NextTrack = 6,
    SeekBackward = 7,
    SeekForward = 8,
    SkipAd = 9,
    Stop = 10,
    SeekTo = 11,
    ScrubTo = 12,
    kMaxValue = ScrubTo,
  };

  MediaSessionUmaHelper();
  ~MediaSessionUmaHelper();

  static void RecordMediaSessionUserAction(MediaSessionUserAction action,
                                           bool focused);

  void RecordSessionSuspended(MediaSessionSuspendedSource source) const;

  // Record the result of calling the native requestAudioFocus().
  void RecordRequestAudioFocusResult(bool result) const;

  void OnSessionActive();
  void OnSessionSuspended();
  void OnSessionInactive();

  void SetClockForTest(const base::TickClock* testing_clock);

 private:
  base::TimeDelta total_active_time_;
  base::TimeTicks current_active_time_;
  const base::TickClock* clock_;
};

}  // namespace content

#endif // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_UMA_HELPER_H_
