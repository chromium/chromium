// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_WEB_STATE_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_WEB_STATE_DELEGATE_H_

/// Handles lifecycle events of the web state associated with the result page.
@protocol LensResultPageWebStateDelegate

/// Called when the web state gets destroyed.
- (void)lensResultPageWebStateDestroyed;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_WEB_STATE_DELEGATE_H_
