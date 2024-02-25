// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_INPUT_EVENT_OBSERVER_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_INPUT_EVENT_OBSERVER_H_

#include <string>

namespace web {
class WebState;
}

// An observer interface implemented by classes which are interested in key
// presses and paste events.
class InputEventObserver {
 public:
  // Called when a key is pressed.
  virtual void OnKeyPressed(std::string text) {}

  // Called when text is pasted.
  virtual void OnPaste(std::string text) {}

  // Returns the WebState for which events are being observed.
  virtual web::WebState* web_state() const = 0;
};

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_INPUT_EVENT_OBSERVER_H_
