// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/dips_service_test_utils.h"

#include "base/logging.h"
#include "content/public/browser/dips_redirect_info.h"

namespace content {
DipsRedirectChainObserver::DipsRedirectChainObserver(DIPSService* service,
                                                     GURL final_url)
    : final_url_(std::move(final_url)) {
  observation_.Observe(service);
}

DipsRedirectChainObserver::~DipsRedirectChainObserver() = default;

void DipsRedirectChainObserver::OnChainHandled(
    const std::vector<DIPSRedirectInfoPtr>& redirects,
    const DIPSRedirectChainInfoPtr& chain) {
  if (chain->final_url.url == final_url_) {
    if (!redirects_.has_value()) {
      redirects_.emplace();
      for (const DIPSRedirectInfoPtr& redirect : redirects) {
        redirects_->push_back(std::make_unique<DIPSRedirectInfo>(*redirect));
      }
    } else {
      LOG(WARNING)
          << "DipsRedirectChainObserver: multiple chains handled ending at "
          << final_url_;
    }
    run_loop_.Quit();
  }
}

void DipsRedirectChainObserver::Wait() {
  run_loop_.Run();
}

}  // namespace content
