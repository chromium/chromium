// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/download_utils_impl.h"

#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_response_headers.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "url/gurl.h"

namespace {

// Allow list to rendering mhtml.
const char* const kAllowListSchemesToRenderingMhtml[] = {
    url::kFileScheme,
#if BUILDFLAG(IS_ANDROID)
    url::kContentScheme,
#endif  // BUILDFLAG(IS_ANDROID)
};

// Determins whether given url would render the mhtml as html according to
// scheme.
bool ShouldAlwaysRenderMhtmlAsHtml(const GURL& url) {
  for (const char* scheme : kAllowListSchemesToRenderingMhtml) {
    if (url.SchemeIs(scheme)) {
      return true;
    }
  }

  return false;
}

}  // namespace

namespace content {
namespace download_utils {

bool MustDownload(BrowserContext* browser_context,
                  const GURL& url,
                  const net::HttpResponseHeaders* headers,
                  const std::string& mime_type) {
  if (headers) {
    std::string disposition;
    if (headers->GetNormalizedHeader("content-disposition", &disposition) &&
        !disposition.empty() &&
        net::HttpContentDisposition(disposition, std::string())
            .is_attachment()) {
      return true;
    }
    if (GetContentClient()->browser()->ShouldForceDownloadResource(
            browser_context, url, mime_type)) {
      return true;
    }
    if (mime_type == "multipart/related" || mime_type == "message/rfc822") {
      // Always allow rendering mhtml for content:// (on Android) and file:///.
      if (ShouldAlwaysRenderMhtmlAsHtml(url)) {
        return false;
      }

      // TODO(crbug.com/40552600): retrieve the new NavigationUIData from
      // the request and and pass it to AllowRenderingMhtmlOverHttp().
      return !GetContentClient()->browser()->AllowRenderingMhtmlOverHttp(
          nullptr);
    }
    // TODO(qinmin): Check whether this is special-case user script that needs
    // to be downloaded.
  }

  return false;
}

bool IsDownload(BrowserContext* browser_context,
                const GURL& url,
                const net::HttpResponseHeaders* headers,
                const std::string& mime_type) {
  if (MustDownload(browser_context, url, headers, mime_type)) {
    return true;
  }

  if (blink::IsSupportedMimeType(mime_type)) {
    return false;
  }

  return !headers || headers->response_code() / 100 == 2;
}

}  // namespace download_utils
}  // namespace content
