// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_UTIL_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_UTIL_H_

#import <Foundation/Foundation.h>

#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ui/base/window_open_disposition.h"

class Browser;
class GURL;

namespace web {
class WebState;
}

// true if `url` can be loaded in an incognito tab.
bool IsURLAllowedInIncognito(const GURL& url);

// Loads `url` in `web_state`, performing any necessary updates to
// `profile`. It is an error to pass a value of GURL that doesn't have a
// javascript: scheme.
void LoadJavaScriptURL(const GURL& url,
                       ProfileIOS* profile,
                       web::WebState* web_state);

// Restores the closed tab identified by `session_id`, using `disposition`,
// into `browser`.
void RestoreTab(const SessionID session_id,
                WindowOpenDisposition disposition,
                Browser* browser);

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_URL_LOADING_UTIL_H_
