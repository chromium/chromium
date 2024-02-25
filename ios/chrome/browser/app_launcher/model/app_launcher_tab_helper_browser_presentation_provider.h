// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_TAB_HELPER_BROWSER_PRESENTATION_PROVIDER_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_TAB_HELPER_BROWSER_PRESENTATION_PROVIDER_H_

// Protocol to retrieve whether the browser is presenting another VC.
@protocol AppLauncherTabHelperBrowserPresentationProvider

// Whether the browser is currently presenting a UI (and so the webState is not
// on top). This UI may or may not occlude the web view.
- (BOOL)isBrowserPresentingUI;

@end

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_TAB_HELPER_BROWSER_PRESENTATION_PROVIDER_H_
