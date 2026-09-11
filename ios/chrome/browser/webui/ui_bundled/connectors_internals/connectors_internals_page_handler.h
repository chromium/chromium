// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_PAGE_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_PAGE_HANDLER_H_

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/types/expected.h"
#import "base/values.h"
#import "components/enterprise/browser/reporting/report_request.h"
#import "components/enterprise/connectors/connectors_internals.mojom.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "mojo/public/cpp/bindings/receiver.h"

namespace enterprise_reporting {
class ChromeProfileRequestGenerator;
enum class ReportGenerationError;
}  // namespace enterprise_reporting

class ConnectorsInternalsPageHandler
    : public connectors_internals::mojom::PageHandler {
 public:
  ConnectorsInternalsPageHandler(
      mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver,
      ProfileIOS* profile);
  ~ConnectorsInternalsPageHandler() override;

  // connectors_internals::mojom::PageHandler:
  void GetDeviceTrustState(GetDeviceTrustStateCallback callback) override;
  void DeleteDeviceTrustKey(DeleteDeviceTrustKeyCallback callback) override;
  void GetClientCertificateState(
      GetClientCertificateStateCallback callback) override;
  void GetSignalsReportingState(
      GetSignalsReportingStateCallback callback) override;
  void GetProvisioningDomainState(
      GetProvisioningDomainStateCallback callback) override;

 private:
  void OnSignalsCollected(GetDeviceTrustStateCallback callback,
                          bool is_device_trust_enabled,
                          base::DictValue signals);
  void OnReportGenerated(
      GetSignalsReportingStateCallback callback,
      connectors_internals::mojom::SignalsReportingStatePtr state,
      base::expected<enterprise_reporting::ReportRequestQueue,
                     enterprise_reporting::ReportGenerationError> result);

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Receiver<connectors_internals::mojom::PageHandler> receiver_;
  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<enterprise_reporting::ChromeProfileRequestGenerator>
      request_generator_;
  base::WeakPtrFactory<ConnectorsInternalsPageHandler> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_PAGE_HANDLER_H_
