// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_VOICE_FOCUSED_WEB_STATE_TRACKER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_VOICE_FOCUSED_WEB_STATE_TRACKER_H_

#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"

class Browser;
class BrowserList;

namespace web {
class WebState;
}  // namespace web

// Tracks the active tab's `web::WebState` in the initial active regular
// `Browser` resolved from `BrowserList`.
class TtcVoiceFocusedWebStateTracker {
 public:
  // Constructs a tracker resolving the active regular `Browser` in
  // `browser_list`, synchronously binding to it (if any). `browser_list` may be
  // `nullptr`.
  explicit TtcVoiceFocusedWebStateTracker(BrowserList* browser_list);
  TtcVoiceFocusedWebStateTracker(const TtcVoiceFocusedWebStateTracker&) =
      delete;
  TtcVoiceFocusedWebStateTracker& operator=(
      const TtcVoiceFocusedWebStateTracker&) = delete;
  ~TtcVoiceFocusedWebStateTracker();

  // Unbinds the tracked browser.
  void Disconnect();

  // Returns the active `WebState` of the currently bound regular `Browser`, or
  // `nullptr` if there is no active tab.
  web::WebState* GetActiveWebState() const;

 private:
  // Binds to `browser` (or unbinds if `nullptr`).
  void BindToBrowser(Browser* browser);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<Browser> bound_browser_;
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_VOICE_FOCUSED_WEB_STATE_TRACKER_H_
