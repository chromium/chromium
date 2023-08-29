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
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {

class WebXrLoggerManager {
 public:
  WebXrLoggerManager();
  ~WebXrLoggerManager();

  WebXrLoggerManager(const WebXrLoggerManager&) = delete;
  WebXrLoggerManager& operator=(const WebXrLoggerManager&) = delete;

  void RecordSessionRequest(
      webxr::mojom::SessionRequestRecordPtr session_request_record);
  void SubscribeToEvents(
      mojo::PendingRemote<webxr::mojom::XRInternalsSessionListener>
          pending_remote);

 private:
  std::vector<webxr::mojom::SessionRequestRecordPtr> session_request_records_;
  mojo::RemoteSet<webxr::mojom::XRInternalsSessionListener> remote_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_XR_WEBXR_INTERNALS_WEBXR_LOGGER_MANAGER_H_
