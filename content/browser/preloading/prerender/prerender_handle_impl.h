// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HANDLE_IMPL_H_
#define CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HANDLE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "content/browser/preloading/prerender/prerender_host.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/prerender_handle.h"

class GURL;

namespace content {

class PrerenderHostRegistry;

class PrerenderHandleImpl final : public PrerenderHandle,
                                  public PrerenderHost::Observer {
 public:
  PrerenderHandleImpl(
      base::WeakPtr<PrerenderHostRegistry> prerender_host_registry,
      PrerenderHostId prerender_host_id,
      const GURL& url,
      std::optional<net::HttpNoVarySearchData> no_vary_search_hint);
  ~PrerenderHandleImpl() override;

  // PrerenderHandle:
  int32_t GetHandleId() const override;
  const GURL& GetInitialPrerenderingUrl() const override;
  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint()
      const override;
  base::WeakPtr<PrerenderHandle> GetWeakPtr() override;
  void SetPreloadingAttemptFailureReason(
      PreloadingFailureReason reason) override;
  void AddActivationCallback(base::OnceClosure activation_callback) override;
  void AddErrorCallback(base::OnceClosure error_callback) override;
  bool IsValid() const override;

  // PrerenderHost::Observer:
  void OnActivated() override;
  void OnFailed(PrerenderFinalStatus status) override;
  void OnHostDestroyed(PrerenderFinalStatus status) override;

  PrerenderHostId prerender_host_id_for_testing() const {
    return prerender_host_id_;
  }

 private:
  const int handle_id_;
  const PrerenderHostId prerender_host_id_;

  base::WeakPtr<PrerenderHostRegistry> prerender_host_registry_;

  const GURL prerendering_url_;
  const std::optional<net::HttpNoVarySearchData> no_vary_search_hint_;

  enum class State { kValid, kActivated, kCanceled };
  State state_ = State::kValid;

  std::vector<base::OnceClosure> activation_callbacks_;
  std::vector<base::OnceClosure> error_callbacks_;

  base::ScopedObservation<PrerenderHost, PrerenderHandleImpl> obs_{this};

  base::WeakPtrFactory<PrerenderHandle> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PRERENDER_PRERENDER_HANDLE_IMPL_H_
