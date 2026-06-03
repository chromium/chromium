// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_

#include <cstdint>
#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/prerender_host_id.h"
#include "net/http/http_no_vary_search_data.h"

class GURL;

namespace content {

// PrerenderLifecycleStatus represents status of the lifecycle events of a
// prerender handle.
enum class PrerenderLifecycleStatus {
  // Headers were received successfully. The prerender is ready for activation.
  kHTTPSuccessResponse,
  // The prerendered page was successfully activated.
  kActivated,
  // Failed due to a bad HTTP response (e.g. 4xx, 5xx).
  kHttpBadResponse,
  // Failed because prerendering was stopped.
  kStop,
  // Failed due to intentional cancellation by embedder or expected lifecycle
  // end.
  kCancelled,
  // Failed due to other reasons (e.g., process crash, low memory).
  kOtherFailure
};

// PrerenderHandle is the class used to encapsulate prerender resources in
// content/. In its destructor, the resource is expected to be released.
class PrerenderHandle {
 public:
  // Observer for PrerenderHandle lifecycle events.
  class CONTENT_EXPORT Observer : public base::CheckedObserver {
   public:
    // Called when the prerender lifecycle status changes.
    // This is guaranteed to be called at least once (with a terminal status).
    virtual void OnLifecycleStateChanged(PrerenderLifecycleStatus status) = 0;
  };

  PrerenderHandle() = default;
  virtual ~PrerenderHandle() = default;

  virtual PrerenderHostId GetPrerenderHostId() const = 0;

  // Returns the initial URL that is passed to PrerenderHostRegistry for
  // starting a prerendering page.
  virtual const GURL& GetInitialPrerenderingUrl() const = 0;

  // Returns the No-Vary-Search hint specified on this prerendering attempt.
  // https://wicg.github.io/nav-speculation/speculation-rules.html#speculation-rule-no-vary-search-hint
  virtual const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint()
      const = 0;

  virtual base::WeakPtr<PrerenderHandle> GetWeakPtr() = 0;
  virtual void SetPreloadingAttemptFailureReason(
      PreloadingFailureReason reason) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns true when prerendering has not been activated or canceled yet.
  virtual bool IsValid() const = 0;

  // Returns true if the prerender is still waiting for its response headers.
  virtual bool IsWaitingForResponseHeaders() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_
