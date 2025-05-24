// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_

#include "base/functional/callback_forward.h"
#include "content/public/browser/preloading_data.h"
#include "net/http/http_no_vary_search_data.h"

namespace content {

// PrerenderHandle is the class used to encapsulate prerender resources in
// content/. In its destructor, the resource is expected to be released.
class PrerenderHandle {
 public:
  PrerenderHandle() = default;
  virtual ~PrerenderHandle() = default;

  virtual int32_t GetHandleId() const = 0;

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

  // Adds a callback to be called on activation. This can be called multiple
  // times.
  virtual void AddActivationCallback(base::OnceClosure activation_callback) = 0;

  // Adds a callback to be called when an error happens. This can be called
  // multiple times.
  virtual void AddErrorCallback(base::OnceClosure error_callback) = 0;

  // Returns true when prerendering has not been activated or canceled yet.
  virtual bool IsValid() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRERENDER_HANDLE_H_
