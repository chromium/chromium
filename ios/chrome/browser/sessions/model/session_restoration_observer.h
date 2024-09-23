// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_OBSERVER_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_OBSERVER_H_

#include <vector>

#include "base/observer_list_types.h"

class Browser;
namespace web {
class WebState;
}  // namespace web

// Observer interface for objects interested in Session restoration events.
class SessionRestorationObserver : public base::CheckedObserver {
 public:
  SessionRestorationObserver() = default;

  // Invoked before the session restoration starts for `browser`.
  virtual void WillStartSessionRestoration(Browser* browser) = 0;

  // Invoked when the session restoration is finished for `browser`.
  virtual void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) = 0;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_OBSERVER_H_
