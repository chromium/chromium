// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/btm_service_test_utils.h"

#include "base/logging.h"
#include "content/browser/btm/btm_bounce_detector.h"
#include "content/public/browser/btm_redirect_info.h"

namespace content {
void Populate3PcExceptions(BrowserContext* browser_context,
                           WebContents* web_contents,
                           const GURL& initial_url,
                           const GURL& final_url,
                           base::span<BtmRedirectInfoPtr> redirects) {
  btm::Populate3PcExceptions(browser_context, web_contents, initial_url,
                             final_url, redirects);
}

bool Are3PcsGenerallyEnabled(BrowserContext* browser_context,
                             WebContents* web_contents) {
  return btm::Are3PcsGenerallyEnabled(browser_context, web_contents);
}

BtmRedirectChainObserver::BtmRedirectChainObserver(BtmService* service,
                                                   GURL final_url)
    : final_url_(std::move(final_url)) {
  observation_.Observe(service);
}

BtmRedirectChainObserver::~BtmRedirectChainObserver() = default;

void BtmRedirectChainObserver::OnChainHandled(
    const std::vector<BtmRedirectInfoPtr>& redirects,
    const BtmRedirectChainInfoPtr& chain) {
  if (chain->final_url.url == final_url_) {
    if (!redirects_.has_value()) {
      redirects_.emplace();
      for (const BtmRedirectInfoPtr& redirect : redirects) {
        redirects_->push_back(std::make_unique<BtmRedirectInfo>(*redirect));
      }
    } else {
      LOG(WARNING)
          << "BtmRedirectChainObserver: multiple chains handled ending at "
          << final_url_;
    }
    run_loop_.Quit();
  }
}

void BtmRedirectChainObserver::Wait() {
  run_loop_.Run();
}

}  // namespace content
