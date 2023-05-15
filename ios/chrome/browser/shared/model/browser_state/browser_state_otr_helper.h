// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_BROWSER_STATE_OTR_HELPER_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_BROWSER_STATE_OTR_HELPER_H_

namespace web {
class BrowserState;
}

// Returns the original BrowserState even for incognito states.
web::BrowserState* GetBrowserStateRedirectedInIncognito(
    web::BrowserState* browser_state);

// Returns non-null BrowserState even for Incognito contexts so that a
// separate instance of a service is created for the Incognito context.
web::BrowserState* GetBrowserStateOwnInstanceInIncognito(
    web::BrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_STATE_BROWSER_STATE_OTR_HELPER_H_
