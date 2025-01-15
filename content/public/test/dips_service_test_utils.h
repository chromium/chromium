// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_DIPS_SERVICE_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_DIPS_SERVICE_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "content/public/browser/dips_service.h"
#include "url/gurl.h"

namespace content {

class DipsRedirectChainObserver : public DIPSService::Observer {
 public:
  DipsRedirectChainObserver(DIPSService* service, GURL final_url);
  ~DipsRedirectChainObserver() override;

  void Wait();
  const std::optional<std::vector<DIPSRedirectInfoPtr>>& redirects() const {
    return redirects_;
  }

 private:
  void OnChainHandled(const std::vector<DIPSRedirectInfoPtr>& redirects,
                      const DIPSRedirectChainInfoPtr& chain) override;

  GURL final_url_;
  base::RunLoop run_loop_;
  std::optional<std::vector<DIPSRedirectInfoPtr>> redirects_;
  base::ScopedObservation<DIPSService, Observer> observation_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_DIPS_SERVICE_TEST_UTILS_H_
