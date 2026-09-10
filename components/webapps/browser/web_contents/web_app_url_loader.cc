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
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
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

// User data attached to a NavigationHandle during WebAppUrlLoader operations to
// enforce URL constraints during navigation and redirect handling.
class WebAppUrlLoaderNavigationHandleData
    : public content::NavigationHandleUserData<
          WebAppUrlLoaderNavigationHandleData> {
 public:
  ~WebAppUrlLoaderNavigationHandleData() override = default;

  const GURL& desired_url() const { return desired_url_; }
  UrlComparison url_comparison() const { return url_comparison_; }
  bool is_redirect_blocked() const { return is_redirect_blocked_; }
  void set_redirect_blocked(bool blocked) { is_redirect_blocked_ = blocked; }

 private:
  friend class content::NavigationHandleUserData<
      WebAppUrlLoaderNavigationHandleData>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();

  WebAppUrlLoaderNavigationHandleData(
      content::NavigationHandle& navigation_handle,
      const GURL& desired_url,
      UrlComparison url_comparison)
      : desired_url_(desired_url), url_comparison_(url_comparison) {}

  const GURL desired_url_;
  const UrlComparison url_comparison_;
  bool is_redirect_blocked_ = false;
};

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(WebAppUrlLoaderNavigationHandleData);

// Navigation throttle that enforces UrlComparison rules before following
// redirects, preventing out-of-scope background navigations.
class WebAppUrlLoaderNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit WebAppUrlLoaderNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      WebAppUrlLoaderNavigationHandleData& data)
      : content::NavigationThrottle(registry), data_(data) {}
  ~WebAppUrlLoaderNavigationThrottle() override = default;

  ThrottleCheckResult WillStartRequest() override {
    return WillStartOrRedirectRequest();
  }

  ThrottleCheckResult WillRedirectRequest() override {
    return WillStartOrRedirectRequest();
  }

  const char* GetNameForLogging() override {
    return "WebAppUrlLoaderNavigationThrottle";
  }

 private:
  ThrottleCheckResult WillStartOrRedirectRequest() {
    const GURL& target_url = navigation_handle()->GetURL();
    if (!target_url.is_valid() || !data_->desired_url().is_valid() ||
        !EqualsWithComparison(target_url, data_->desired_url(),
                              data_->url_comparison())) {
      data_->set_redirect_blocked(true);
      return content::NavigationThrottle::CANCEL;
    }

    return content::NavigationThrottle::PROCEED;
  }

  const raw_ref<WebAppUrlLoaderNavigationHandleData> data_;
};

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

  void LoadUrl(content::NavigationController::LoadURLParams load_params,
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

    web_contents->GetController().LoadURLWithParams(std::move(load_params));

    timer_.Start(FROM_HERE, WebAppUrlLoader::kSecondsToWaitForWebContentsLoad,
                 base::BindOnce(&LoaderTask::OnLoadUrlTimeout,
                                // OneShotTimer is owned by this class and
                                // it guarantees that it will never run after
                                // it's destroyed.
                                base::Unretained(this)));
  }

  // WebContentsObserver
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (WebContentsShuttingDown(web_contents())) {
      PostResultTask(WebAppUrlLoader::Result::kFailedWebContentsDestroyed);
      return;
    }

    if (navigation_handle->IsInPrimaryMainFrame()) {
      WebAppUrlLoaderNavigationHandleData::CreateForNavigationHandle(
          *navigation_handle, url_, url_comparison_);
    }
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (WebContentsShuttingDown(web_contents())) {
      PostResultTask(WebAppUrlLoader::Result::kFailedWebContentsDestroyed);
      return;
    }

    if (!navigation_handle->IsInPrimaryMainFrame()) {
      return;
    }

    if (!navigation_handle->HasCommitted()) {
      auto* data = WebAppUrlLoaderNavigationHandleData::GetForNavigationHandle(
          *navigation_handle);
      if (data && data->is_redirect_blocked()) {
        LOG(ERROR) << "Error loading " << url_
                   << "  page redirected to unexpected destination.";
        PostResultTask(WebAppUrlLoader::Result::kRedirectedUrlLoaded);
        return;
      }
    }
  }
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

    LOG(ERROR) << "Error loading " << url_ << "  page failed to load.";
    PostResultTask(WebAppUrlLoader::Result::kFailedUnknownReason);
  }

  void WebContentsDestroyed() override {
    PostResultTask(WebAppUrlLoader::Result::kFailedWebContentsDestroyed);
  }

 private:
  void OnLoadUrlTimeout() {
    web_contents()->Stop();
    LOG(ERROR) << "Error loading " << url_ << " page took too long to load.";
    PostResultTask(WebAppUrlLoader::Result::kFailedPageTookTooLong);
  }

  void PostResultTask(WebAppUrlLoader::Result result) {
    timer_.Stop();
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

std::ostream& operator<<(std::ostream& os, WebAppUrlLoaderResult result) {
  switch (result) {
    case WebAppUrlLoaderResult::kUrlLoaded:
      return os << "kUrlLoaded";
    case WebAppUrlLoaderResult::kRedirectedUrlLoaded:
      return os << "kRedirectedUrlLoaded";
    case WebAppUrlLoaderResult::kFailedUnknownReason:
      return os << "kFailedUnknownReason";
    case WebAppUrlLoaderResult::kFailedPageTookTooLong:
      return os << "kFailedPageTookTooLong";
    case WebAppUrlLoaderResult::kFailedWebContentsDestroyed:
      return os << "kFailedWebContentsDestroyed";
    case WebAppUrlLoaderResult::kFailedErrorPageLoaded:
      return os << "kFailedErrorPageLoaded";
  }
}

WebAppUrlLoader::WebAppUrlLoader() = default;

WebAppUrlLoader::~WebAppUrlLoader() = default;

// static
void WebAppUrlLoader::MaybeCreateAndAddNavigationThrottle(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame()) {
    return;
  }
  if (auto* data =
          WebAppUrlLoaderNavigationHandleData::GetForNavigationHandle(handle)) {
    registry.AddThrottle(
        std::make_unique<WebAppUrlLoaderNavigationThrottle>(registry, *data));
  }
}

void WebAppUrlLoader::LoadUrl(
    content::NavigationController::LoadURLParams load_url_params,
    content::WebContents* web_contents,
    UrlComparison url_comparison,
    ResultCallback callback) {
  CHECK(web_contents);
  PrepareForLoad(
      web_contents,
      base::BindOnce(
          &WebAppUrlLoader::LoadUrlInternal, weak_factory_.GetWeakPtr(),
          std::move(load_url_params), web_contents->GetWeakPtr(),
          url_comparison,
          base::BindOnce(&WebAppUrlLoader::OnUrlLoaded,
                         weak_factory_.GetWeakPtr(),
                         "Webapp.WebAppUrlLoaderResult", std::move(callback))));
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
  LoadUrlInternal(
      std::move(load_params), web_contents->GetWeakPtr(), UrlComparison::kExact,
      base::BindOnce(&WebAppUrlLoader::OnUrlLoaded, weak_factory_.GetWeakPtr(),
                     "Webapp.WebAppUrlLoaderPrepareForLoadResult",
                     base::IgnoreArgs<Result>(std::move(complete))));
}

void WebAppUrlLoader::LoadUrlInternal(
    content::NavigationController::LoadURLParams load_url_params,
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
  if (!load_url_params.initiator_origin.has_value() &&
      load_url_params.url.is_valid() && !load_url_params.url.IsAboutBlank()) {
    load_url_params.initiator_origin = url::Origin::Create(load_url_params.url);
  }
  auto loader_task = std::make_unique<LoaderTask>();
  auto* loader_task_ptr = loader_task.get();
  loader_task_ptr->LoadUrl(
      std::move(load_url_params), web_contents.get(), url_comparison,
      base::BindOnce(
          [](std::unique_ptr<LoaderTask> task, Result result) {
            task.reset();
            return result;
          },
          std::move(loader_task))
          .Then(std::move(callback)));
}

void WebAppUrlLoader::OnUrlLoaded(std::string_view metrics_name,
                                  ResultCallback callback,
                                  WebAppUrlLoaderResult result) {
  base::UmaHistogramEnumeration(metrics_name, result);
  std::move(callback).Run(result);
}

}  // namespace webapps
