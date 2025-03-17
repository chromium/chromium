// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BTM_SERVICE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BTM_SERVICE_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "content/public/browser/btm_service.h"
#include "url/gurl.h"

namespace content {

class DipsRedirectChainObserver : public BtmService::Observer {
 public:
  DipsRedirectChainObserver(BtmService* service, GURL final_url);
  ~DipsRedirectChainObserver() override;

  void Wait();
  const std::optional<std::vector<BtmRedirectInfoPtr>>& redirects() const {
    return redirects_;
  }

 private:
  void OnChainHandled(const std::vector<BtmRedirectInfoPtr>& redirects,
                      const BtmRedirectChainInfoPtr& chain) override;

  GURL final_url_;
  base::RunLoop run_loop_;
  std::optional<std::vector<BtmRedirectInfoPtr>> redirects_;
  base::ScopedObservation<BtmService, Observer> observation_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BTM_SERVICE_TEST_UTILS_H_
