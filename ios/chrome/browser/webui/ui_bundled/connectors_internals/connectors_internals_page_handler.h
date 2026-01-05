// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_PAGE_HANDLER_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_PAGE_HANDLER_H_

#import "base/memory/raw_ptr.h"
#import "components/enterprise/connectors/connectors_internals.mojom.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "mojo/public/cpp/bindings/receiver.h"

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

 private:
  mojo::Receiver<connectors_internals::mojom::PageHandler> receiver_;
  raw_ptr<ProfileIOS> profile_;
};

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_CONNECTORS_INTERNALS_CONNECTORS_INTERNALS_PAGE_HANDLER_H_
