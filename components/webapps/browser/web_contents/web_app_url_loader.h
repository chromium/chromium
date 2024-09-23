// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_WEB_CONTENTS_WEB_APP_URL_LOADER_H_
#define COMPONENTS_WEBAPPS_BROWSER_WEB_CONTENTS_WEB_APP_URL_LOADER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/navigation_controller.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace webapps {

// Result enum values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused. Update corresponding enums.xml
// entry when making changes here.
enum class WebAppUrlLoaderResult {
  // The provided URL (matched using |UrlComparison|) was loaded.
  kUrlLoaded = 0,
  // The provided URL redirected to another URL (that did not match using
  // |UrlComparison|) and the final URL was loaded.
  kRedirectedUrlLoaded = 1,
  kFailedUnknownReason = 2,
  kFailedPageTookTooLong = 3,
  kFailedWebContentsDestroyed = 4,
  kFailedErrorPageLoaded = 5,

  kMaxValue = kFailedErrorPageLoaded,
};

// Callback-based wrapper around NavigationController::LoadUrl.
class WebAppUrlLoader {
 public:
  // How to compare resolved URL against the given URL for the purpose of
  // determining a successful load.
  enum class UrlComparison {
    kExact,
    kIgnoreQueryParamsAndRef,
    kSameOrigin,
  };

  // Exposed for testing.
  static constexpr base::TimeDelta kSecondsToWaitForWebContentsLoad =
      base::Seconds(30);

  using Result = WebAppUrlLoaderResult;

  using ResultCallback = base::OnceCallback<void(Result)>;

  WebAppUrlLoader();
  virtual ~WebAppUrlLoader();

  // Navigates `web_contents` to about:blank, followed by navigating to `url`.
  // This prevents races with events from old contents loaded in the
  // `web_contents`. After navigating to `url`, the `url` is compared with the
  // resolved URL with `url_comparison`, and runs callback with the result code.
  virtual void LoadUrl(const GURL& url,
                       content::WebContents* web_contents,
                       UrlComparison url_comparison,
                       ResultCallback callback);

  // Navigates `web_contents` to about:blank, followed by navigating based on
  // `load_url_params`. This prevents races with events from old contents loaded
  // in the `web_contents`. Compares the resolved URL with `url_comparison`, and
  // runs callback with the result code.
  virtual void LoadUrl(
      content::NavigationController::LoadURLParams load_url_params,
      content::WebContents* web_contents,
      UrlComparison url_comparison,
      ResultCallback callback);

  // Used by LoadUrl() to put `web_contents` into a clean state, will noop if
  // called redundantly. Useful for other uses of `web_contents` e.g.
  // downloading icons.
  virtual void PrepareForLoad(content::WebContents* web_contents,
                              base::OnceClosure complete);

 private:
  void LoadUrlInternal(
      const content::NavigationController::LoadURLParams& load_url_params,
      base::WeakPtr<content::WebContents> web_contents,
      UrlComparison url_comparison,
      ResultCallback callback);

  base::WeakPtrFactory<WebAppUrlLoader> weak_factory_{this};
};

const char* ConvertUrlLoaderResultToString(WebAppUrlLoader::Result result);

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_BROWSER_WEB_CONTENTS_WEB_APP_URL_LOADER_H_
