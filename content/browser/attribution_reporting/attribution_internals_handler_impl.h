// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_

#include "content/browser/attribution_reporting/conversion_internals.mojom.h"
#include "content/browser/attribution_reporting/conversion_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class WebUI;

// Implements the mojo endpoint for the Conversion WebUI which proxies calls to
// the ConversionManager to get information about stored conversion data. Owned
// by ConversionInternalsUI.
class AttributionInternalsHandlerImpl
    : public mojom::ConversionInternalsHandler {
 public:
  AttributionInternalsHandlerImpl(
      WebUI* web_ui,
      mojo::PendingReceiver<mojom::ConversionInternalsHandler> receiver);
  AttributionInternalsHandlerImpl(
      const AttributionInternalsHandlerImpl& other) = delete;
  AttributionInternalsHandlerImpl& operator=(
      const AttributionInternalsHandlerImpl& other) = delete;
  AttributionInternalsHandlerImpl(AttributionInternalsHandlerImpl&& other) =
      delete;
  AttributionInternalsHandlerImpl& operator=(
      AttributionInternalsHandlerImpl&& other) = delete;
  ~AttributionInternalsHandlerImpl() override;

  // mojom::ConversionInternalsHandler overrides:
  void IsMeasurementEnabled(
      mojom::ConversionInternalsHandler::IsMeasurementEnabledCallback callback)
      override;
  void GetActiveImpressions(
      mojom::ConversionInternalsHandler::GetActiveImpressionsCallback callback)
      override;
  void GetReports(
      mojom::ConversionInternalsHandler::GetReportsCallback callback) override;
  void SendPendingReports(
      mojom::ConversionInternalsHandler::SendPendingReportsCallback callback)
      override;
  void ClearStorage(mojom::ConversionInternalsHandler::ClearStorageCallback
                        callback) override;

  void SetConversionManagerProviderForTesting(
      std::unique_ptr<ConversionManager::Provider> manager_provider);

 private:
  WebUI* web_ui_;
  std::unique_ptr<ConversionManager::Provider> manager_provider_;

  mojo::Receiver<mojom::ConversionInternalsHandler> receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
