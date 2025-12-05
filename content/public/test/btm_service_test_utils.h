// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BTM_SERVICE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BTM_SERVICE_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "content/public/browser/btm_redirect.h"
#include "content/public/browser/btm_service.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;

void Populate3PcExceptions(BrowserContext* browser_context,
                           WebContents* web_contents,
                           const GURL& initial_url,
                           const GURL& final_url,
                           base::span<BtmRedirectPtr> redirects);

bool Are3PcsGenerallyEnabled(BrowserContext* browser_context,
                             WebContents* web_contents);

class BtmRedirectChainObserver : public BtmService::Observer {
 public:
  BtmRedirectChainObserver(BtmService* service, GURL final_url);
  ~BtmRedirectChainObserver() override;

  void Wait();
  const std::optional<std::vector<BtmRedirectPtr>>& redirects() const {
    return redirects_;
  }

 private:
  void OnChainHandled(const std::vector<BtmRedirectPtr>& redirects,
                      const BtmRedirectChainPtr& chain) override;

  GURL final_url_;
  base::RunLoop run_loop_;
  std::optional<std::vector<BtmRedirectPtr>> redirects_;
  base::ScopedObservation<BtmService, Observer> observation_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BTM_SERVICE_TEST_UTILS_H_
