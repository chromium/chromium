// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_MATCH_RESOLVER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_MATCH_RESOLVER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle_user_data.h"

namespace content {

class PrefetchContainer;

// TODO(crbug.com/1462206): Problem: how do we inform this class of prefetches
// being started while we are waiting for existing in-progress prefetches ?
// PrefetchService should probably do it.
class CONTENT_EXPORT PrefetchMatchResolver
    : public NavigationHandleUserData<PrefetchMatchResolver> {
 public:
  PrefetchMatchResolver(const PrefetchMatchResolver&) = delete;
  PrefetchMatchResolver& operator=(const PrefetchMatchResolver&) = delete;
  ~PrefetchMatchResolver() override;

  base::WeakPtr<PrefetchMatchResolver> GetWeakPtr();

  using OnPrefetchToServeReady =
      base::OnceCallback<void(PrefetchContainer::Reader prefetch_to_serve)>;
  void SetOnPrefetchToServeReadyCallback(
      OnPrefetchToServeReady on_prefetch_to_serve_ready);

  // A prefetch can be served, so let the browser know that it can use the
  // prefetch for the navigation.
  void PrefetchServed(PrefetchContainer::Reader reader);
  // The prefetch container / prefetch_url cannot be used. If there are no
  // more potential prefetches to wait for, let the browser know to fallback
  // to normal navigation.
  void PrefetchNotUsable(const PrefetchContainer& prefetch_container);
  void PrefetchNotUsable(const GURL& prefetch_url);
  // A prefetch is not available so let the browser know to fallback to regular
  // navigation instead.
  void PrefetchNotAvailable();
  // If Cookies have changed, then none of the matched prefetches can be served.
  // Remove all of the prefetches from `in_progress_prefetch_matches_` and let
  // the browser know to fallback to regular navigation instead.
  void FallbackToRegularNavigationWhenMatchedPrefetchCookiesChanged(
      PrefetchContainer& prefetch_container);
  void WaitForPrefetch(PrefetchContainer& prefetch_container);
  void EndWaitForPrefetch(const GURL& prefetch_url);
  // Check if we are waiting already for the head of this `prefetch_container`.
  bool IsWaitingForPrefetch(const PrefetchContainer& prefetch_container) const;
  bool IsWaitingForPrefetch(const GURL& prefetch_url) const;

 private:
  friend NavigationHandleUserData<PrefetchMatchResolver>;
  explicit PrefetchMatchResolver(NavigationHandle& navigation_handle);

  void MaybeFallbackToRegularNavigationWhenPrefetchNotUsable();
  bool IsWaitingOnPrefetchHead() const;

  OnPrefetchToServeReady ReleaseOnPrefetchToServeReadyCallback();

  // Once the prefetch (if any) that can be used to serve a navigation to
  // |url| is identified, this callback is called with that
  // prefetch.
  OnPrefetchToServeReady on_prefetch_to_serve_ready_callback_;

  // Keep track of all prefetches that we are waiting for head on.
  std::map<GURL, base::WeakPtr<PrefetchContainer>>
      in_progress_prefetch_matches_;

  base::WeakPtrFactory<PrefetchMatchResolver> weak_ptr_factory_{this};

  // For debug logs.
  CONTENT_EXPORT friend std::ostream& operator<<(
      std::ostream& ostream,
      const PrefetchMatchResolver& prefetch_match_resolver);
  friend NavigationHandleUserData;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_MATCH_RESOLVER_H_
