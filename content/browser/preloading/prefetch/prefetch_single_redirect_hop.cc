// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_single_redirect_hop.h"

#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

namespace content {

PrefetchSingleRedirectHop::PrefetchSingleRedirectHop(
    PrefetchContainer& prefetch_container,
    const GURL& url,
    bool is_isolated_network_context_required,
    perfetto::Flow flow)
    : url_(url),
      is_isolated_network_context_required_(
          is_isolated_network_context_required),
      response_reader_(base::MakeRefCounted<PrefetchResponseReader>(
          base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                         prefetch_container.GetWeakPtr()),
          base::BindOnce(&PrefetchContainer::OnPrefetchComplete,
                         prefetch_container.GetWeakPtr()),
          std::move(flow))),
      prefetch_container_(prefetch_container) {}

PrefetchSingleRedirectHop::~PrefetchSingleRedirectHop() {
  CHECK(response_reader_);
  base::SequencedTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(response_reader_));
}

void PrefetchSingleRedirectHop::RegisterCookieListener() {
  if (!is_isolated_network_context_required()) {
    return;
  }

  cookie_listener_ = PrefetchCookieListener::MakeAndRegister(
      url_, prefetch_container_->request()
                .browser_context()
                ->GetDefaultStoragePartition()
                ->GetCookieManagerForBrowserProcess());
}

bool PrefetchSingleRedirectHop::HaveDefaultContextCookiesChanged() const {
  if (cookie_listener_) {
    return cookie_listener_->HaveCookiesChanged();
  }
  return false;
}

void PrefetchSingleRedirectHop::PauseCookieListener() {
  if (cookie_listener_) {
    cookie_listener_->PauseListening();
  }
}

void PrefetchSingleRedirectHop::ResumeCookieListener() {
  if (cookie_listener_) {
    cookie_listener_->ResumeListening();
  }
}

}  // namespace content
