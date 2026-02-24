// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_HANDLE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "net/cookies/cookie_util.h"

class GURL;

namespace content {

class PrefetchContainer;
class PrefetchMatchResolverAction;
class PrefetchResponseReader;
class PrefetchSingleRedirectHop;
class ServiceWorkerClient;
enum class PrefetchProbeResult;
enum class PrefetchServableState;
enum class PrefetchStatus;

// A `PrefetchServingHandle` represents the current state of serving
// for a `PrefetchContainer`. The `PrefetchServingHandle` methods all
// operate on the currently *serving* `PrefetchSingleRedirectHop`,
// which is the element in |PrefetchContainer::redirect_chain()| at index
// |index_redirect_chain_to_serve_|.
//
// This works like `base::WeakPtr<PrefetchContainer>` plus additional states,
// so check that the reader is valid (e.g. `if (reader)`) before calling other
// methods (except for `Clone()`).
class CONTENT_EXPORT PrefetchServingHandle final {
 public:
  PrefetchServingHandle();

  PrefetchServingHandle(base::WeakPtr<PrefetchContainer> prefetch_container,
                        size_t index_redirect_chain_to_serve);

  PrefetchServingHandle(const PrefetchServingHandle&) = delete;
  PrefetchServingHandle& operator=(const PrefetchServingHandle&) = delete;

  PrefetchServingHandle(PrefetchServingHandle&&);
  PrefetchServingHandle& operator=(PrefetchServingHandle&&);

  ~PrefetchServingHandle();

  const PrefetchContainer* GetPrefetchContainer() const {
    return prefetch_container_.get();
  }
  PrefetchContainer* GetPrefetchContainer() {
    return prefetch_container_.get();
  }
  PrefetchServingHandle Clone();

  // Returns true if `this` is valid.
  // Do not call methods below if false.
  bool IsValid() const { return GetPrefetchContainer(); }
  explicit operator bool() const { return IsValid(); }

  // Methods redirecting to `GetPrefetchContainer()`.
  PrefetchMatchResolverAction GetMatchResolverAction() const;

  bool HasPrefetchStatus() const;
  PrefetchStatus GetPrefetchStatus() const;

  // Returns whether `this` reached the end. If true, the methods below
  // shouldn't be called, because the current `SingleRedirectHop` doesn't exist.
  bool IsEnd() const;

  // Whether or not an isolated network context is required to serve.
  bool IsIsolatedNetworkContextRequiredToServe() const;

  bool HaveDefaultContextCookiesChanged() const;

  bool IsIsolatedCookieCopyInProgressForTesting() const;
  void OnIsolatedCookieCopyStartForTesting();
  void OnIsolatedCookiesReadCompleteAndWriteStartForTesting();
  void OnIsolatedCookieCopyCompleteForTesting();
  void OnInterceptorCheckCookieCopyForTesting();
  void SetOnCookieCopyCompleteCallbackForTesting(base::OnceClosure callback);

  // Called with the result of the probe. If the probing feature is enabled,
  // then a probe must complete successfully before the prefetch can be
  // served.
  void OnPrefetchProbeResult(PrefetchProbeResult probe_result);

  // Checks if the given URL matches the the URL that can be served next.
  bool DoesCurrentURLToServeMatch(const GURL& url) const;

  // Returns the URL that can be served next.
  const GURL& GetCurrentURLToServe() const;

  // Gets the current PrefetchResponseReader.
  base::WeakPtr<PrefetchResponseReader>
  GetCurrentResponseReaderToServeForTesting();

  // Called when one element of |redirect_chain_| is served and the next
  // element can now be served.
  void AdvanceCurrentURLToServe() { index_redirect_chain_to_serve_++; }

  // See the comment for `PrefetchResponseReader::CreateRequestHandler()`.
  std::pair<PrefetchRequestHandler, base::WeakPtr<ServiceWorkerClient>>
  CreateRequestHandler();

  // See the corresponding functions on `PrefetchResponseReader`.
  // These apply to the current `SingleRedirectHop` (and so, may change as the
  // prefetch advances through a redirect change).
  bool VariesOnCookieIndices() const;
  bool MatchesCookieIndices(
      base::span<const std::pair<std::string, std::string>> cookies) const;

  // Checks if `prefetch_container` can be used for the url of intercepted
  // `tentative_resource_request`, and starts checking `PrefetchOriginProber` if
  // needed.
  void OnGotPrefetchToServe(
      FrameTreeNodeId frame_tree_node_id,
      const GURL& tentative_resource_request_url,
      base::OnceCallback<void(PrefetchServingHandle)> get_prefetch_callback) &&;

  // Copies any cookies in the isolated network context associated with
  // the current redirect hop to the default network context.
  void CopyIsolatedCookies();

 private:
  const std::vector<std::unique_ptr<PrefetchSingleRedirectHop>>&
  redirect_chain() const;

  // Returns the `SingleRedirectHop` to be served next.
  const PrefetchSingleRedirectHop& GetCurrentSingleRedirectHopToServe() const;
  PrefetchSingleRedirectHop& GetCurrentSingleRedirectHopToServe();

  // Validation methods.
  struct OnGotPrefetchToServeState;
  void ContinueOnGotPrefetchToServe(
      std::unique_ptr<OnGotPrefetchToServeState> state) &&;
  void StartCookieValidation(
      std::unique_ptr<OnGotPrefetchToServeState> state) &&;
  void OnGotCookiesForValidation(
      std::unique_ptr<OnGotPrefetchToServeState> state,
      const std::vector<net::CookieWithAccessResult>& cookies,
      const std::vector<net::CookieWithAccessResult>& excluded_cookies) &&;
  void OnProbeComplete(std::unique_ptr<OnGotPrefetchToServeState> state,
                       base::TimeTicks probe_start_time,
                       PrefetchProbeResult probe_result) &&;
  void OnCookieCopyComplete(std::unique_ptr<OnGotPrefetchToServeState> state,
                            base::TimeTicks cookie_copy_start_time) &&;

  base::WeakPtr<PrefetchContainer> prefetch_container_;

  // The index of the element in |GetPrefetchContainer()->redirect_chain()| that
  // can be served.
  size_t index_redirect_chain_to_serve_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_HANDLE_H_
