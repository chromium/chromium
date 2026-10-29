// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_EGTEST_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_EGTEST_UTIL_H_

namespace autofill {

// Gives native keyboard focus to the web element with `element_id` in an
// autofill form.
//
// `TapWebElementWithId()` computes its tap point once, when the action is
// created, from the element rect, the web view zoom scale, and the content
// inset. Right after a page load (in particular when the page has no viewport
// meta tag, like `profile_form.html`) or an autofill keyboard transition,
// `WKWebView` may still be applying its scale and insets, so a single tap can
// miss the input field. This helper therefore scrolls the element into view,
// taps it with a freshly computed point (`TapWebElementUnverified()`), and
// waits for the single-tap gesture to complete until
// `document.activeElement?.id` matches `element_id`.
void FocusWebElementWithId(const char* element_id);

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_AUTOFILL_EGTEST_UTIL_H_
