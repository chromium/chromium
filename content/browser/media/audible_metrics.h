// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIBLE_METRICS_H_
#define CONTENT_BROWSER_MEDIA_AUDIBLE_METRICS_H_

#include <list>
#include <memory>
#include <set>

#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "content/common/content_export.h"

namespace content {

class WebContents;

// This class handles metrics regarding audible WebContents.
// It records four different histograms:
// - how many WebContents are audible when a WebContents become audible.
// - how long multiple WebContents are audible at the same time.
// - for a browsing session, how often and how many WebContents get audible at
//   the same time.
// - if an audible tab was closed and we only have one audible tab open anymore
//   then we should record whether we closed the newest or oldest tab.
class CONTENT_EXPORT AudibleMetrics {
 public:
  // This is used for recording whether the audible tab that exited concurrent
  // playback was the most recent. It is used for recording a histogram so
  // values should not be changed.
  enum class ExitConcurrentPlaybackContents {
    kMostRecent = 0,
    kOlder = 1,
    kMaxValue = kOlder,
  };

  AudibleMetrics();
  ~AudibleMetrics();

  void UpdateAudibleWebContentsState(const WebContents* web_contents,
                                     bool audible);
  void WebContentsDestroyed(const WebContents* web_contents,
                            bool recently_audible);

  void SetClockForTest(const base::TickClock* test_clock);

  int GetAudibleWebContentsSizeForTest() const {
    return audible_web_contents_.size();
  }

 private:
  void AddAudibleWebContents(const WebContents* web_contents);
  void RemoveAudibleWebContents(const WebContents* web_contents);

  base::TimeTicks concurrent_web_contents_start_time_;
  size_t max_concurrent_audible_web_contents_in_session_;
  const base::TickClock* clock_;

  // This stores the audible web contents in insertion order. We add a
  // web contents to the list when it becomes audible and remove it is
  // destroyed.
  std::list<const WebContents*> last_audible_web_contents_;

  // Stores all the web contents that are currently audible. We add a web
  // contents to the set when it becomes currently audible and remove it when it
  // is no longer audible.
  std::set<const WebContents*> audible_web_contents_;

  DISALLOW_COPY_AND_ASSIGN(AudibleMetrics);
};

}  // namespace content

#endif // CONTENT_BROWSER_MEDIA_AUDIBLE_METRICS_H_
