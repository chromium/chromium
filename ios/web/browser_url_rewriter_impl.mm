// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/browser_url_rewriter_impl.h"

#import "base/check.h"
#import "base/no_destructor.h"
#import "base/strings/string_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_client.h"
#import "url/gurl.h"

namespace web {

namespace {

// The scheme used to view the source of a page using WebUI in content/.
const char kViewSourceScheme[] = "view-source";

// Handles rewriting view-source URLs for what we'll actually load.  Since
// WebUI-based view-source isn't supported on iOS, simply strip out the scheme
// and load the URL.  This is to gracefully handle tabs synced from other
// platforms with the "view-source:" scheme.
static bool HandleViewSource(GURL* url, BrowserState* browser_state) {
  DCHECK(url);
  if (url->SchemeIs(kViewSourceScheme)) {
    // Load the inner URL instead.
    *url = GURL(url->GetContent());
    return true;
  }
  return false;
}
}  // namespace

// static
BrowserURLRewriter* BrowserURLRewriter::GetInstance() {
  return BrowserURLRewriterImpl::GetInstance();
}

// static
bool BrowserURLRewriter::RewriteURLWithWriters(
    GURL* url,
    web::BrowserState* browser_state,
    const std::vector<BrowserURLRewriter::URLRewriter>& rewriters) {
  bool rewritten = false;
  for (URLRewriter rewriter : rewriters) {
    if ((rewritten = rewriter(url, browser_state)))
      break;
  }
  return rewritten;
}

// static
BrowserURLRewriterImpl* BrowserURLRewriterImpl::GetInstance() {
  static base::NoDestructor<BrowserURLRewriterImpl> instance;
  return instance.get();
}

BrowserURLRewriterImpl::BrowserURLRewriterImpl() {
  web::WebClient* web_client = web::GetWebClient();
  if (web_client)
    web_client->PostBrowserURLRewriterCreation(this);

  // view-source:
  AddURLRewriter(&HandleViewSource);
}

BrowserURLRewriterImpl::~BrowserURLRewriterImpl() {}

void BrowserURLRewriterImpl::AddURLRewriter(URLRewriter rewriter) {
  DCHECK(rewriter);
  url_rewriters_.push_back(rewriter);
}

bool BrowserURLRewriterImpl::RewriteURLIfNecessary(
    GURL* url,
    BrowserState* browser_state) {
  return BrowserURLRewriter::RewriteURLWithWriters(url, browser_state,
                                                   url_rewriters_);
}

}  // namespace web
