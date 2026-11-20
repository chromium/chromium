// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_WEB_STATE_KEYBOARD_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_WEB_STATE_KEYBOARD_UTIL_H_

namespace web {
class WebState;
}

// Dismisses the keyboard if a field in `web_state`'s view is being edited.
// Does nothing if `web_state` is null or not realized.
void DismissKeyboardForWebState(web::WebState* web_state);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_UTILS_WEB_STATE_KEYBOARD_UTIL_H_
