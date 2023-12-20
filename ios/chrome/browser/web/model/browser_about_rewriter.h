// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_BROWSER_ABOUT_REWRITER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_BROWSER_ABOUT_REWRITER_H_

class GURL;

namespace web {
class BrowserState;
}

// If the url has the "about:" scheme (excluding "about:blank"), it will be
// rewritten with the "chrome:" scheme.  Returns true if the browser about
// handler will handle the url.  This is a web::BrowserURLRewriter::URLRewriter
// function that is used by web::BrowserURLRewriter.
bool WillHandleWebBrowserAboutURL(GURL* url, web::BrowserState* browser_state);

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_BROWSER_ABOUT_REWRITER_H_
