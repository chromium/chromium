// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_ACTIVITY_OVERLAY_EGTEST_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_ACTIVITY_OVERLAY_EGTEST_UTIL_H_

// Waits for the activity overlay to disappear.  You should call it at
// the ends of an EG test that uses activity overlay, to ensure the
// overlay does not remains on the next next.
void WaitForActivityOverlayToDisappear();

#endif  // IOS_CHROME_BROWSER_SHARED_UI_ELEMENTS_ACTIVITY_OVERLAY_EGTEST_UTIL_H_
