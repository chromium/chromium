// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_WEB_STATE_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_WEB_STATE_DELEGATE_H_

namespace web {
class WebState;
}  // namespace web

/// Handles lifecycle events of the web state associated with the result page.
@protocol LensResultPageWebStateDelegate

/// Called when the web state gets destroyed.
- (void)lensResultPageWebStateDestroyed;

/// Called when the active `webState` in LensResultPageMediator changes.
- (void)lensResultPageDidChangeActiveWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_WEB_STATE_DELEGATE_H_
