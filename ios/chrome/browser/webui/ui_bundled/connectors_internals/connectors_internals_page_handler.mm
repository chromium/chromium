// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/connectors_internals/connectors_internals_page_handler.h"

ConnectorsInternalsPageHandler::ConnectorsInternalsPageHandler(
    mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver,
    ProfileIOS* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

ConnectorsInternalsPageHandler::~ConnectorsInternalsPageHandler() = default;

void ConnectorsInternalsPageHandler::GetDeviceTrustState(
    GetDeviceTrustStateCallback callback) {
  auto state = connectors_internals::mojom::DeviceTrustState::New(
      false, std::vector<std::string>(),
      connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED,
          nullptr,
          connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED),
      std::string(), nullptr);
  std::move(callback).Run(std::move(state));
}

void ConnectorsInternalsPageHandler::DeleteDeviceTrustKey(
    DeleteDeviceTrustKeyCallback callback) {
  std::move(callback).Run();
}

void ConnectorsInternalsPageHandler::GetClientCertificateState(
    GetClientCertificateStateCallback callback) {
  std::move(callback).Run(
      connectors_internals::mojom::ClientCertificateState::New(
          std::vector<std::string>(), nullptr, nullptr));
}

void ConnectorsInternalsPageHandler::GetSignalsReportingState(
    GetSignalsReportingStateCallback callback) {
  std::move(callback).Run(
      connectors_internals::mojom::SignalsReportingState::New(
          /*error_info=*/"User signals reporting is unsupported on the current "
                         "platform",
          /*status_report_enabled=*/false, /*signals_report_enabled=*/false,
          /*last_upload_attempt_timestamp=*/std::string(),
          /*last_upload_success_timestamp=*/std::string(),
          /*last_signals_upload_config=*/std::string(),
          /*can_collect_all_fields=*/false));
}
