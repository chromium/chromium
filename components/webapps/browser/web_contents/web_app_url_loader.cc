// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/web_contents/web_app_url_loader.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace webapps {
namespace {
using UrlComparison = WebAppUrlLoader::UrlComparison;

bool EqualsWithComparison(const GURL& a,
                          const GURL& b,
                          UrlComparison url_comparison) {
  DCHECK(a.is_valid());
  DCHECK(b.is_valid());
  if (a == b) {
    return true;
  }
  GURL::Replacements replace;
  switch (url_comparison) {
    case UrlComparison::kExact:
      return false;
    case UrlComparison::kSameOrigin:
      replace.ClearPath();
      [[fallthrough]];
    case UrlComparison::kIgnoreQueryParamsAndRef:
      replace.ClearQuery();
      replace.ClearRef();
      break;
  }
  return a.ReplaceComponents(replace) == b.ReplaceComponents(replace);
}

// TODO(b/302531937): Make this a utility that can be used through out the
// web_applications/ system.
bool WebContentsShuttingDown(content::WebContents* web_contents) {
  return !web_contents || web_contents->IsBeingDestroyed() ||
         web_contents->GetBrowserContext()->ShutdownStarted();
}

class LoaderTask : public content::WebContentsObserver {
 public:
  LoaderTask() = default;
  LoaderTask(const LoaderTask&) = delete;
  LoaderTask& operator=(const LoaderTask&) = delete;
  LoaderTask(LoaderTask&&) = delete;
  LoaderTask& operator=(LoaderTask&&) = delete;
  ~LoaderTask() override = default;

  void LoadUrl(const content::NavigationController::LoadURLParams& load_params,
               content::WebContents* web_contents,
               UrlComparison url_comparison,
               WebAppUrlLoader::ResultCallback callback) {
    url_ = load_params.url;
    url_comparison_ = url_comparison;
    callback_ = std::move(callback);
    Observe(web_contents);

    if (WebContentsShuttingDown(web_contents)) {
      PostResultTask(WebAppUrlLoader::Result::kFailedWebContentsDestroyed);
      return;
    }

    web_contents->GetController().LoadURLWithParams(load_params);

    timer_.Start(FROM_HERE, WebAppUrlLoader::kSecondsToWaitForWebContentsLoad,
                 base::BindOnce(&LoaderTask::OnLoadUrlTimeout,
                                // OneShotTimer is owned by this class and
                                // it guarantees that it will never run after
                                // it's destroyed.
                                base::Unretained(this)));
  }

  // WebContentsObserver
  // DidFinishLoad doesn't always get called after the page has fully loaded.
  // TODO(ortuno): Use DidStopLoading instead.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (WebContentsShuttingDown(web_contents())) {
      PostResultTask(WebAppUrlLoader::Result::kFailedWebContentsDestroyed);
      return;
    }

    if (IsSubframeLoad(render_frame_host)) {
      return;
    }

    // Flush all DidFinishLoad events until about:blank loaded.
    if ((url_.IsAboutBlank() && !validated_url.IsAboutBlank()) ||
        (!url_.IsAboutBlank() && validated_url.IsAboutBlank())) {
      return;
    }

    timer_.Stop();

    if (validated_url == content::kUnreachableWebDataURL) {
      // Navigation ends up in an error page. For example, network errors and
      // policy blocked URLs.
      PostResultTask(WebAppUrlLoader::Result::kFailedErrorPageLoaded);
      return;
    }

    const network::mojom::URLResponseHead* response_head =
        render_frame_host->GetLastResponseHead();
    if (response_head && response_head->headers &&
        response_head->headers->response_code() != net::HTTP_OK) {
      // Navigation loads content but is not successful. For example, HTTP-500
      // class of errors.
      PostResultTask(WebAppUrlLoader::Result::kFailedErrorPageLoaded);
      return;
    }

    if (EqualsWithComparison(validated_url, url_, url_comparison_)) {
      PostResultTask(WebAppUrlLoader::Result::kUrlLoaded);
      return;
    }
    LOG(ERROR) << "Error loading " << url_ << "  page redirected to "
               << validated_url;
    PostResultTask(WebAppUrlLoader::Result::kRedirectedUrlLoaded);
  }

  bool IsSubframeLoad(content::RenderFrameHost* render_frame_host) const {
    return !render_frame_host->IsInPrimaryMainFrame();
  }

  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override {
    if (WebContentsShuttingDown(web_contents())) {
      PostResultTask(WebAppUrlLoader::Result::kFailedWebContentsDestroyed);
      return;
    }

    if (IsSubframeLoad(render_frame_host)) {
      return;
    }

    // Flush all DidFailLoad events until about:blank loaded.
    if (url_.IsAboutBlank()) {
      return;
    }

    timer_.Stop();

    LOG(ERROR) << "Error loading " << url_ << "  page failed to load.";
    PostResultTask(WebAppUrlLoader::Result::kFailedUnknownReason);
  }

  void WebContentsDestroyed() override {
    timer_.Stop();
    PostResultTask(WebAppUrlLoader::Result::kFailedWebContentsDestroyed);
  }

 private:
  void OnLoadUrlTimeout() {
    web_contents()->Stop();
    LOG(ERROR) << "Error loading " << url_ << " page took too long to load.";
    PostResultTask(WebAppUrlLoader::Result::kFailedPageTookTooLong);
  }

  void PostResultTask(WebAppUrlLoader::Result result) {
    Observe(nullptr);
    // Post a task to avoid reentrancy issues e.g. adding a WebContentsObserver
    // while a previous observer call is being executed.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), result));
  }

  GURL url_;
  UrlComparison url_comparison_;
  WebAppUrlLoader::ResultCallback callback_;

  base::OneShotTimer timer_;

  base::WeakPtrFactory<LoaderTask> weak_ptr_factory_{this};
};

}  // namespace

WebAppUrlLoader::WebAppUrlLoader() = default;

WebAppUrlLoader::~WebAppUrlLoader() = default;

void WebAppUrlLoader::LoadUrl(
    content::NavigationController::LoadURLParams load_url_params,
    content::WebContents* web_contents,
    UrlComparison url_comparison,
    ResultCallback callback) {
  CHECK(web_contents);
  PrepareForLoad(
      web_contents,
      base::BindOnce(&WebAppUrlLoader::LoadUrlInternal,
                     weak_factory_.GetWeakPtr(), std::move(load_url_params),
                     web_contents->GetWeakPtr(), url_comparison,
                     std::move(callback)));
}

void WebAppUrlLoader::LoadUrl(const GURL& url,
                              content::WebContents* web_contents,
                              UrlComparison url_comparison,
                              ResultCallback callback) {
  content::NavigationController::LoadURLParams load_params(url);
  load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  LoadUrl(std::move(load_params), web_contents, url_comparison,
          std::move(callback));
}

void WebAppUrlLoader::PrepareForLoad(content::WebContents* web_contents,
                                     base::OnceClosure complete) {
  if (web_contents->GetLastCommittedURL().IsAboutBlank() &&
      web_contents->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(complete));
    return;
  }

  content::NavigationController::LoadURLParams load_params{
      GURL(url::kAboutBlankURL)};
  load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  LoadUrlInternal(load_params, web_contents->GetWeakPtr(),
                  UrlComparison::kExact,
                  base::BindOnce([](Result result) {
                    base::UmaHistogramEnumeration(
                        "Webapp.WebAppUrlLoaderPrepareForLoadResult", result);
                  }).Then(std::move(complete)));
}

void WebAppUrlLoader::LoadUrlInternal(
    const content::NavigationController::LoadURLParams& load_url_params,
    base::WeakPtr<content::WebContents> web_contents,
    UrlComparison url_comparison,
    ResultCallback callback) {
  if (WebContentsShuttingDown(web_contents.get())) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       WebAppUrlLoader::Result::kFailedWebContentsDestroyed));
    return;
  }
  auto loader_task = std::make_unique<LoaderTask>();
  auto* loader_task_ptr = loader_task.get();
  loader_task_ptr->LoadUrl(
      load_url_params, web_contents.get(), url_comparison,
      base::BindOnce(
          [](std::unique_ptr<LoaderTask> task, Result result) {
            task.reset();
            return result;
          },
          std::move(loader_task))
          .Then(std::move(callback)));
}

const char* ConvertUrlLoaderResultToString(WebAppUrlLoader::Result result) {
  using Result = WebAppUrlLoader::Result;
  switch (result) {
    case Result::kUrlLoaded:
      return "UrlLoaded";
    case Result::kRedirectedUrlLoaded:
      return "RedirectedUrlLoaded";
    case Result::kFailedUnknownReason:
      return "FailedUnknownReason";
    case Result::kFailedPageTookTooLong:
      return "FailedPageTookTooLong";
    case Result::kFailedWebContentsDestroyed:
      return "FailedWebContentsDestroyed";
    case Result::kFailedErrorPageLoaded:
      return "FailedErrorPageLoaded";
  }
}

}  // namespace web_app
