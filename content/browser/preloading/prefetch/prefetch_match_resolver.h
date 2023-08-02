// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_MATCH_RESOLVER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_MATCH_RESOLVER_H_

#include <vector>

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

  bool HasExactPrefetchMatch() const;
  void SetExactPrefetchMatch(PrefetchContainer& prefetch);
  PrefetchContainer* GetExactPrefetchMatch() const;

  bool HasInexactPrefetchMatch() const;
  void AddInexactPrefetchMatch(PrefetchContainer& prefetch);
  std::vector<PrefetchContainer*> GetInexactPrefetchMatches() const;

  base::WeakPtr<PrefetchMatchResolver> GetWeakPtr();

  using OnPrefetchToServeReady =
      base::OnceCallback<void(PrefetchContainer::Reader prefetch_to_serve)>;
  void SetOnPrefetchToServeReadyCallback(
      OnPrefetchToServeReady on_prefetch_to_serve_ready);
  OnPrefetchToServeReady ReleaseOnPrefetchToServeReadyCallback();

 private:
  friend NavigationHandleUserData<PrefetchMatchResolver>;
  explicit PrefetchMatchResolver(NavigationHandle& navigation_handle);

  // Once the prefetch (if any) that can be used to serve a navigation to
  // |url| is identified, this callback is called with that
  // prefetch.
  OnPrefetchToServeReady on_prefetch_to_serve_ready_callback_;

  // There is only one exact prefetch match that we should be waiting for.
  base::WeakPtr<PrefetchContainer> exact_prefetch_match_;

  // Keep track of all inexact prefetch potential matches that we should be
  // waiting for.
  std::vector<base::WeakPtr<PrefetchContainer>> inexact_prefetch_matches_;

  base::WeakPtrFactory<PrefetchMatchResolver> weak_ptr_factory_{this};

  friend NavigationHandleUserData;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_MATCH_RESOLVER_H_
