// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_single_redirect_hop.h"

#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"

namespace content {

PrefetchSingleRedirectHop::PrefetchSingleRedirectHop(
    PrefetchContainer& prefetch_container,
    const GURL& url,
    bool is_isolated_network_context_required)
    : url_(url),
      is_isolated_network_context_required_(
          is_isolated_network_context_required),
      response_reader_(base::MakeRefCounted<PrefetchResponseReader>(
          base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                         prefetch_container.GetWeakPtr()),
          base::BindOnce(&PrefetchContainer::OnPrefetchComplete,
                         prefetch_container.GetWeakPtr()))) {}

PrefetchSingleRedirectHop::~PrefetchSingleRedirectHop() {
  CHECK(response_reader_);
  base::SequencedTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(response_reader_));
}

}  // namespace content
