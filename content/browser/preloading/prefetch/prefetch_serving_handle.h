// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_HANDLE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/common/content_export.h"

class GURL;

namespace content {

class PrefetchContainer;
class PrefetchNetworkContext;
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

  PrefetchContainer* GetPrefetchContainer() const {
    return prefetch_container_.get();
  }
  PrefetchServingHandle Clone() const;

  // Returns true if `this` is valid.
  // Do not call methods below if false.
  explicit operator bool() const { return GetPrefetchContainer(); }

  // Methods redirecting to `GetPrefetchContainer()`.
  PrefetchServableState GetServableState(
      base::TimeDelta cacheable_duration) const;
  bool HasPrefetchStatus() const;
  PrefetchStatus GetPrefetchStatus() const;

  // Returns whether `this` reached the end. If true, the methods below
  // shouldn't be called, because the current `SingleRedirectHop` doesn't exist.
  bool IsEnd() const;

  // Whether or not an isolated network context is required to serve.
  bool IsIsolatedNetworkContextRequiredToServe() const;

  PrefetchNetworkContext* GetCurrentNetworkContextToServe() const;

  bool HaveDefaultContextCookiesChanged() const;

  // Before a prefetch can be served, any cookies added to the isolated
  // network context must be copied over to the default network context. These
  // functions are used to check and update the status of this process, as
  // well as record metrics about how long this process takes.
  bool HasIsolatedCookieCopyStarted() const;
  bool IsIsolatedCookieCopyInProgress() const;
  void OnIsolatedCookieCopyStart() const;
  void OnIsolatedCookiesReadCompleteAndWriteStart() const;
  void OnIsolatedCookieCopyComplete() const;
  void OnInterceptorCheckCookieCopy() const;
  void SetOnCookieCopyCompleteCallback(base::OnceClosure callback) const;

  // Called with the result of the probe. If the probing feature is enabled,
  // then a probe must complete successfully before the prefetch can be
  // served.
  void OnPrefetchProbeResult(PrefetchProbeResult probe_result) const;

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

 private:
  const std::vector<std::unique_ptr<PrefetchSingleRedirectHop>>&
  redirect_chain() const;

  // Returns the `SingleRedirectHop` to be served next.
  const PrefetchSingleRedirectHop& GetCurrentSingleRedirectHopToServe() const;

  base::WeakPtr<PrefetchContainer> prefetch_container_;

  // The index of the element in |GetPrefetchContainer()->redirect_chain()| that
  // can be served.
  size_t index_redirect_chain_to_serve_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVING_HANDLE_H_
