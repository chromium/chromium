// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_H_
#define IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/safe_browsing/core/browser/client_side_detection_service_base.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace safe_browsing {

// iOS implementation of `ClientSideDetectionService`.
// This class is responsible for holding the phishing model and sending
// phishing reports to Safe Browsing servers. The browser-side `Scorer` used
// for in-process classification is owned by `ClientSideDetectionServiceBase`
// (see `GetScorer()`).
class ClientSideDetectionService : public ClientSideDetectionServiceBase {
 public:
  ClientSideDetectionService(
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      optimization_guide::OptimizationGuideModelProvider* opt_guide);

  ClientSideDetectionService(const ClientSideDetectionService&) = delete;
  ClientSideDetectionService& operator=(const ClientSideDetectionService&) =
      delete;

  ~ClientSideDetectionService() override;

  // Sets the `SharedURLLoaderFactory` for testing.
  void SetSharedURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 private:
  friend class ClientSideDetectionServiceTest;

  // `ClientSideDetectionServiceBase` overrides:
  void DidSendClientReportPhishingRequest(
      std::unique_ptr<ClientPhishingRequest> request,
      const std::string& access_token) override;
  void DidReceiveClientPhishingResponse(
      const ClientPhishingResponse& response) override;
  bool ShouldCreateBrowserScorer() const override;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace safe_browsing

#endif  // IOS_CHROME_BROWSER_SAFE_BROWSING_MODEL_CLIENT_SIDE_DETECTION_CLIENT_SIDE_DETECTION_SERVICE_H_
