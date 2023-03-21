// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;
class PrefetchContainer;
class PrefetchOriginProber;

// Intercepts navigations that can use prefetched resources.
class CONTENT_EXPORT PrefetchURLLoaderInterceptor
    : public NavigationLoaderInterceptor {
 public:
  static std::unique_ptr<PrefetchURLLoaderInterceptor> MaybeCreateInterceptor(
      int frame_tree_node_id);

  explicit PrefetchURLLoaderInterceptor(int frame_tree_node_id);
  ~PrefetchURLLoaderInterceptor() override;

  PrefetchURLLoaderInterceptor(const PrefetchURLLoaderInterceptor&) = delete;
  PrefetchURLLoaderInterceptor& operator=(const PrefetchURLLoaderInterceptor&) =
      delete;

  // NavigationLoaderInterceptor
  void MaybeCreateLoader(
      const network::ResourceRequest& tenative_resource_request,
      BrowserContext* browser_context,
      NavigationLoaderInterceptor::LoaderCallback callback,
      NavigationLoaderInterceptor::FallbackCallback fallback_callback) override;

 private:
  // Gets the prefetch associated with |url| from |PrefetchService|. The
  // |get_prefetch_callback| is called with this associated prefetch.
  virtual void GetPrefetch(
      const GURL& url,
      base::OnceCallback<void(base::WeakPtr<PrefetchContainer>)>
          get_prefetch_callback) const;

  // Gets the relevant |GetPrefetchOriginProber| from |PrefetchService|.
  virtual PrefetchOriginProber* GetPrefetchOriginProber() const;

  // Checks the prefetch retrieved via |GetPrefetch| to see if it can be used
  // for |tenative_resource_request|.
  void OnGotPrefetchToServe(
      const network::ResourceRequest& tenative_resource_request,
      base::WeakPtr<PrefetchContainer> prefetch_container);

  void OnProbeComplete(base::WeakPtr<PrefetchContainer> prefetch_container,
                       base::OnceClosure on_success_callback,
                       PrefetchProbeResult result);

  // Ensures that the cookies for prefetch are copied from its isolated network
  // context to the default network context before calling
  // |InterceptPrefetchedNavigation|.
  void EnsureCookiesCopiedAndInterceptPrefetchedNavigation(
      const network::ResourceRequest& tenative_resource_request,
      base::WeakPtr<PrefetchContainer> prefetch_container);

  // Starts the cookie copy for next redirect hop of |prefetch_container|.
  virtual void StartCookieCopy(
      base::WeakPtr<PrefetchContainer> prefetch_container);

  void InterceptPrefetchedNavigation(
      const network::ResourceRequest& tenative_resource_request,
      base::WeakPtr<PrefetchContainer> prefetch_container);
  void DoNotInterceptNavigation();

  // The frame tree node |this| is associated with, used to retrieve
  // |PrefetchService|.
  const int frame_tree_node_id_;

  // The URL being navigated to.
  GURL url_;

  // Called once |this| has decided whether to intercept or not intercept the
  // navigation.
  NavigationLoaderInterceptor::LoaderCallback loader_callback_;

  // The time when probing was started. Used to calculate probe latency which is
  // reported to the tab helper.
  absl::optional<base::TimeTicks> probe_start_time_;

  // Result of the most recent probe.
  PrefetchProbeResult probe_result_{PrefetchProbeResult::kNoProbing};

  // The time when we started waiting for cookies to be copied, delaying the
  // navigation. Used to calculate total cookie wait time.
  absl::optional<base::TimeTicks> cookie_copy_start_time_;

  // The prefetch container that has already been used to serve a redirect. If
  // another request can be intercepted, this will be checked first to see if
  // its next redirect hop matches the request URL.
  base::WeakPtr<PrefetchContainer> redirect_prefetch_container_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchURLLoaderInterceptor> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_URL_LOADER_INTERCEPTOR_H_
