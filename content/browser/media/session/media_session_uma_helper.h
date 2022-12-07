// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_UMA_HELPER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_UMA_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace base {
class TickClock;
}  // namespace base

namespace content {

class CONTENT_EXPORT MediaSessionUmaHelper {
 public:
  // This is used for UMA histogram (Media.Session.Suspended). New values should
  // be appended only and must be added before |Count|.
  enum class MediaSessionSuspendedSource {
    kSystemTransient = 0,
    kSystemPermanent = 1,
    kUI = 2,
    kCONTENT = 3,
    kSystemTransientDuck = 4,
    kMaxValue = kSystemTransientDuck,
  };

  // Extended enum to media_session::mojom::MediaSessionAction, distinguishing
  // default action handling.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class MediaSessionUserAction {
    kPlay = 0,
    kPlayDefault = 1,
    kPause = 2,
    kPauseDefault = 3,
    kStopDefault = 4,
    kPreviousTrack = 5,
    kNextTrack = 6,
    kSeekBackward = 7,
    kSeekForward = 8,
    kSkipAd = 9,
    kStop = 10,
    kSeekTo = 11,
    kScrubTo = 12,
    kEnterPictureInPicture = 13,
    kExitPictureInPicture = 14,
    kSwitchAudioDevice = 15,
    kToggleMicrophone = 16,
    kToggleCamera = 17,
    kHangUp = 18,
    kRaise = 19,
    kSetMute = 20,
    kPreviousSlide = 21,
    kNextSlide = 22,
    kMaxValue = kNextSlide,
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
  raw_ptr<const base::TickClock> clock_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_UMA_HELPER_H_
