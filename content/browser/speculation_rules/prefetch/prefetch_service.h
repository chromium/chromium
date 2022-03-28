// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_SERVICE_H_
#define CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_SERVICE_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/speculation_rules/prefetch/prefetch_container.h"
#include "content/browser/speculation_rules/prefetch/prefetch_status.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "net/cookies/canonical_cookie.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;
class ServiceWorkerContext;

// Manages all prefetches within a single BrowserContext. Responsible for
// checking the eligibility of the prefetch, making the network request for the
// prefetch, and provide prefetched resources to URL loader interceptor when
// needed.
class CONTENT_EXPORT PrefetchService {
 public:
  // |browser_context| must outlive this instance. In general this should always
  // be true, since |PrefetchService| will be indirectly owned by
  // |BrowserContext|.
  static std::unique_ptr<PrefetchService> CreateIfPossible(
      BrowserContext* browser_context);

  explicit PrefetchService(BrowserContext* browser_context);
  ~PrefetchService();

  PrefetchService(const PrefetchService&) = delete;
  const PrefetchService& operator=(const PrefetchService&) = delete;

  void PrefetchUrl(base::WeakPtr<PrefetchContainer> prefetch_container);

  // Helper functions to control the behavior of the eligibility check when
  // testing.
  static void SetServiceWorkerContextForTesting(ServiceWorkerContext* context);
  static void SetHostNonUniqueFilterForTesting(
      bool (*filter)(base::StringPiece));

 private:
  // Checks whether the given |prefetch_container| is eligible for prefetch.
  // Once the eligibility is determined then |result_callback| will be called
  // with result and an optional status stating why the prefetch is not
  // eligible.
  using OnEligibilityResultCallback =
      base::OnceCallback<void(base::WeakPtr<PrefetchContainer>,
                              bool eligible,
                              absl::optional<PrefetchStatus> status)>;
  void CheckEligibilityOfPrefetch(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      OnEligibilityResultCallback result_callback) const;

  // Called after getting the existing cookies associated with
  // |prefetch_container|. If there are any cookies, then the prefetch is not
  // eligible.
  void OnGotCookiesForEligibilityCheck(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      OnEligibilityResultCallback result_callback,
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies) const;

  // Called once the eligibility of |prefetch_container| is determined. If the
  // prefetch is eligible it is added to the queue to be prefetched. If it is
  // not eligible, then we consider making it a decoy request.
  void OnGotEligibilityResult(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      bool eligible,
      absl::optional<PrefetchStatus> status) const;

  raw_ptr<BrowserContext> browser_context_;

  std::map<PrefetchContainer::Key, base::WeakPtr<PrefetchContainer>>
      all_prefetches_;

  base::WeakPtrFactory<PrefetchService> weak_method_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPECULATION_RULES_PREFETCH_PREFETCH_SERVICE_H_
