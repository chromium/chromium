// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SINGLE_REDIRECT_HOP_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SINGLE_REDIRECT_HOP_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "net/cookies/cookie_util.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"
#include "url/gurl.h"

namespace content {

class PrefetchContainer;
class PrefetchCookieListener;
class PrefetchResponseReader;

// Holds the state for the request for a single URL in the context of the
// broader prefetch. A prefetch can request multiple URLs due to redirects.
// const/mutable member convention:
// ------------------------ ----------- -------
// can be modified during:  prefetching serving
// ------------------------ ----------- -------
// const                    No          No
// non-const/non-mutable    Yes         No
// mutable                  Yes         Yes
// ------------------------ ----------- -------
// because const references are used via `GetCurrentSingleRedirectHopToServe()`
// and other const-qualified member functions during serving.
//
// This is semi-internal class of `PrefetchContainer` that should be only
// exposed to a limited number of its associated classes.
//
// TODO(https://crbug.com/437416134): Consider making the fields private and set
// proper methods and access control. Currently all methods used during serving
// is marked as const to follow the convention above, but instead `mutable`
// should be removed and state-modifying methods should be marked non-const.
class CONTENT_EXPORT PrefetchSingleRedirectHop final {
 public:
  PrefetchSingleRedirectHop(PrefetchContainer& prefetch_container,
                            const GURL& url,
                            perfetto::Flow flow);
  ~PrefetchSingleRedirectHop();

  PrefetchSingleRedirectHop(const PrefetchSingleRedirectHop&) = delete;
  PrefetchSingleRedirectHop& operator=(const PrefetchSingleRedirectHop&) =
      delete;

  const GURL& url() const { return url_; }
  bool is_isolated_network_context_required() const {
    return is_isolated_network_context_required_;
  }
  const PrefetchResponseReader& response_reader() const {
    return *response_reader_;
  }
  PrefetchResponseReader& response_reader() { return *response_reader_; }

  // Registers a cookie listener for this prefetch if it is using an isolated
  // network context. If the cookies in the default partition associated with
  // this URL change after this point, then the prefetched resources should
  // not be served.
  void RegisterCookieListener();
  bool HaveDefaultContextCookiesChanged() const;
  void PauseCookieListener();
  void ResumeCookieListener();

  // Copies any cookies in the isolated network context associated with `this`
  // to the default network context.
  void CopyIsolatedCookies();

  // Before a prefetch can be served, any cookies added to the isolated
  // network context must be copied over to the default network context. These
  // functions are used to check and update the status of this process, as
  // well as record metrics about how long this process takes.
  bool IsIsolatedCookieCopyInProgress() const;
  void SetOnCookieCopyCompleteCallback(base::OnceClosure callback);

  void OnInterceptorCheckCookieCopy();
  void OnIsolatedCookieCopyStart();
  void OnIsolatedCookiesReadCompleteAndWriteStart();
  void OnIsolatedCookieCopyComplete();

  // Called with the `PrefetchContainer`'s initial URL and the currently serving
  // URL.
  using OnIsolatedCookieCopyStartCallbackForTesting =
      base::RepeatingCallback<void(const GURL&, const GURL&)>;
  static void SetOnIsolatedCookieCopyStartCallbackForTesting(
      OnIsolatedCookieCopyStartCallbackForTesting
          on_isolated_cookie_copy_start_callback_for_testing);

 private:
  // Isolated cookie copy methods.
  bool HasIsolatedCookieCopyStarted() const;
  // Called when the cookies are read from the isolated network context for
  // `this` (== the isolated network context of `prefetch_container_`) and are
  // ready to be written to the default network context.
  void OnGotIsolatedCookiesForCopy(
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies);

  // The URL that will potentially be prefetched. This can be the original
  // prefetch URL, or a URL from a redirect resulting from requesting the
  // original prefetch URL.
  const GURL url_;

  const bool is_isolated_network_context_required_;

  // This tracks whether the cookies associated with |url_| have changed at
  // some point after the initial eligibility check.
  std::unique_ptr<PrefetchCookieListener> cookie_listener_;

  scoped_refptr<PrefetchResponseReader> response_reader_;

  // The different possible states of the cookie copy process.
  enum class CookieCopyStatus {
    kNotStarted,
    kInProgress,
    kCompleted,
  };

  // The current state of the cookie copy process for this prefetch.
  CookieCopyStatus cookie_copy_status_ = CookieCopyStatus::kNotStarted;

  // The timestamps of when the overall cookie copy process starts, and midway
  // when the cookies are read from the isolated network context and are about
  // to be written to the default network context.
  std::optional<base::TimeTicks> cookie_copy_start_time_;
  std::optional<base::TimeTicks> cookie_read_end_and_write_start_time_;

  // A callback that runs once |cookie_copy_status_| is set to |kCompleted|.
  base::OnceClosure on_cookie_copy_complete_callback_;

  // The `PrefetchContainer` owning `this`.
  raw_ref<PrefetchContainer> prefetch_container_;

  base::WeakPtrFactory<PrefetchSingleRedirectHop> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SINGLE_REDIRECT_HOP_H_
