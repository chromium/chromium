// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_url_handler_impl.h"

#include <stddef.h>

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "url/gurl.h"

namespace content {

// Handles rewriting view-source URLs for what we'll actually load.
static bool HandleViewSource(GURL* url, BrowserContext* browser_context) {
  if (url->SchemeIs(kViewSourceScheme)) {
    // Load the inner URL instead.
    *url = GURL(url->GetContent());

    // Bug 26129: limit view-source to view the content and not any
    // other kind of 'active' url scheme like 'javascript' or 'data'.
    static const char* const default_allowed_sub_schemes[] = {
        url::kHttpScheme,
        url::kHttpsScheme,
        url::kFtpScheme,
        kChromeUIScheme,
        url::kFileScheme,
        url::kFileSystemScheme
    };

    // Merge all the schemes for which view-source is allowed by default, with
    // the view-source schemes defined by the ContentBrowserClient.
    std::vector<std::string> all_allowed_sub_schemes;
    for (size_t i = 0; i < base::size(default_allowed_sub_schemes); ++i)
      all_allowed_sub_schemes.push_back(default_allowed_sub_schemes[i]);
    GetContentClient()->browser()->GetAdditionalViewSourceSchemes(
        &all_allowed_sub_schemes);

    bool is_sub_scheme_allowed = false;
    for (size_t i = 0; i < all_allowed_sub_schemes.size(); ++i) {
      if (url->SchemeIs(all_allowed_sub_schemes[i].c_str())) {
        is_sub_scheme_allowed = true;
        break;
      }
    }

    if (!is_sub_scheme_allowed) {
      *url = GURL(url::kAboutBlankURL);
      return false;
    }

    return true;
  }
  return false;
}

// Turns a non view-source URL into the corresponding view-source URL.
static bool ReverseViewSource(GURL* url, BrowserContext* browser_context) {
  // No action necessary if the URL is already view-source:
  if (url->SchemeIs(kViewSourceScheme))
    return false;
  // Recreate the url with the view-source scheme.
  *url = GURL(kViewSourceScheme + std::string(":") + url->spec());
  return true;
}

static bool DebugURLHandler(GURL* url, BrowserContext* browser_context) {
  // Circumvent processing URLs that the renderer process will handle.
  return IsRendererDebugURL(*url);
}

// static
BrowserURLHandler* BrowserURLHandler::GetInstance() {
  return BrowserURLHandlerImpl::GetInstance();
}

// static
BrowserURLHandler::URLHandler BrowserURLHandler::null_handler() {
  // Required for VS2010: http://connect.microsoft.com/VisualStudio/feedback/details/520043/error-converting-from-null-to-a-pointer-type-in-std-pair
  return nullptr;
}

// static
BrowserURLHandlerImpl* BrowserURLHandlerImpl::GetInstance() {
  return base::Singleton<BrowserURLHandlerImpl>::get();
}

BrowserURLHandlerImpl::BrowserURLHandlerImpl() :
    fixup_handler_(nullptr) {
  AddHandlerPair(&DebugURLHandler, BrowserURLHandlerImpl::null_handler());

  // view-source: should take precedence over other rewriters, so it's
  // important to add it before calling up to the content client.
  AddHandlerPair(&HandleViewSource, &ReverseViewSource);

  GetContentClient()->browser()->BrowserURLHandlerCreated(this);
}

BrowserURLHandlerImpl::~BrowserURLHandlerImpl() {
}

void BrowserURLHandlerImpl::SetFixupHandler(URLHandler handler) {
  DCHECK(fixup_handler_ == nullptr);
  fixup_handler_ = handler;
}

void BrowserURLHandlerImpl::AddHandlerPair(URLHandler handler,
                                           URLHandler reverse_handler) {
  url_handlers_.push_back(HandlerPair(handler, reverse_handler));
}

void BrowserURLHandlerImpl::RewriteURLIfNecessary(
    GURL* url,
    BrowserContext* browser_context,
    bool* reverse_on_redirect) {
  for (size_t i = 0; i < url_handlers_.size(); ++i) {
    URLHandler handler = *url_handlers_[i].first;
    if (handler && handler(url, browser_context)) {
      *reverse_on_redirect = (url_handlers_[i].second != NULL);
      return;
    }
  }
}

void BrowserURLHandlerImpl::FixupURLBeforeRewrite(
    GURL* url,
    BrowserContext* browser_context) {
  if (fixup_handler_)
    fixup_handler_(url, browser_context);
}

bool BrowserURLHandlerImpl::ReverseURLRewrite(
    GURL* url, const GURL& original, BrowserContext* browser_context) {
  for (size_t i = 0; i < url_handlers_.size(); ++i) {
    URLHandler reverse_rewriter = *url_handlers_[i].second;
    if (reverse_rewriter) {
      GURL test_url(original);
      URLHandler handler = *url_handlers_[i].first;
      if (!handler) {
        if (reverse_rewriter(url, browser_context))
          return true;
      } else if (handler(&test_url, browser_context)) {
        return reverse_rewriter(url, browser_context);
      }
    }
  }
  return false;
}

void BrowserURLHandlerImpl::SetFixupHandlerForTesting(URLHandler handler) {
  fixup_handler_ = handler;
}

}  // namespace content
