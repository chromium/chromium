// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_UMA_HELPER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_UMA_HELPER_H_

#include <optional>

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
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class EnterPictureInPictureType {
    // EnterPictureInPicture was called for the default handler provided by
    // MediaSessionImpl.
    kDefaultManual = 0,

    // EnterPictureInPicture was called for an enterpictureinpicture handler
    // provided by the website.
    kRegisteredManual = 1,

    // EnterAutoPictureInPicture was called for an enterpictureinpicture handler
    // provided by the website.
    kRegisteredAutomatic = 2,

    // EnterAutoPictureInPicture was called for the default handler provided by
    // MediaSessionImpl.
    kDefaultAutomatic = 3,

    kMaxValue = kDefaultAutomatic,
  };

  MediaSessionUmaHelper();
  ~MediaSessionUmaHelper();

  void RecordEnterPictureInPicture(EnterPictureInPictureType type) const;

  void OnSessionActive();
  void OnSessionSuspended();
  void OnSessionInactive();
  void OnServiceDestroyed();
  void OnMediaPictureInPictureChanged(bool is_picture_in_picture);

  void SetClockForTest(const base::TickClock* testing_clock);

 private:
  base::TimeDelta total_active_time_;
  base::TimeTicks current_active_time_;
  std::optional<base::TimeTicks> current_enter_pip_time_ = std::nullopt;
  std::optional<base::TimeDelta> total_pip_time_for_session_ = std::nullopt;
  raw_ptr<const base::TickClock> clock_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_UMA_HELPER_H_
