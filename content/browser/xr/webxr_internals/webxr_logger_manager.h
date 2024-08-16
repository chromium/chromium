// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_XR_WEBXR_INTERNALS_WEBXR_LOGGER_MANAGER_H_
#define CONTENT_BROWSER_XR_WEBXR_INTERNALS_WEBXR_LOGGER_MANAGER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/browser/xr/webxr_internals/mojom/webxr_internals.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_device.mojom-shared.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {

class WebXrLoggerManager
    : public device::mojom::WebXrInternalsRendererListener {
 public:
  WebXrLoggerManager();
  ~WebXrLoggerManager() override;

  WebXrLoggerManager(const WebXrLoggerManager&) = delete;
  WebXrLoggerManager& operator=(const WebXrLoggerManager&) = delete;

  void RecordSessionRequested(
      webxr::mojom::SessionRequestedRecordPtr session_requested_record);
  void RecordSessionRejected(
      webxr::mojom::SessionRejectedRecordPtr session_rejected_record);
  void RecordSessionStarted(
      webxr::mojom::SessionStartedRecordPtr session_started_record);
  void RecordSessionStopped(
      webxr::mojom::SessionStoppedRecordPtr session_stopped_record);
  // Functions that do not send historical data.
  void RecordRuntimeAdded(webxr::mojom::RuntimeInfoPtr runtime_added_record);
  void RecordRuntimeRemoved(device::mojom::XRDeviceId device_id);

  void SubscribeToEvents(
      mojo::PendingRemote<webxr::mojom::XRInternalsSessionListener>
          pending_remote);

  mojo::PendingRemote<device::mojom::WebXrInternalsRendererListener>
  BindRenderListener();

  // WebXrInternalsRendererListener
  void OnFrameData(
      device::mojom::XrFrameStatisticsPtr xrframe_statistics) override;

  void OnConsoleLog(
      device::mojom::XrLogMessagePtr xr_logging_statistics) override;

 private:
  std::vector<webxr::mojom::SessionRequestedRecordPtr>
      session_requested_records_;
  std::vector<webxr::mojom::SessionRejectedRecordPtr> session_rejected_records_;
  std::vector<webxr::mojom::SessionStartedRecordPtr> session_started_records_;
  std::vector<webxr::mojom::SessionStoppedRecordPtr> session_stopped_records_;

  mojo::RemoteSet<webxr::mojom::XRInternalsSessionListener> remote_set_;
  mojo::Receiver<device::mojom::WebXrInternalsRendererListener>
      renderer_listener_receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_WEBXR_INTERNALS_WEBXR_LOGGER_MANAGER_H_
