// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/webxr_internals/webxr_logger_manager.h"

#include "base/time/time.h"
#include "content/browser/xr/webxr_internals/mojom/webxr_internals.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {

WebXrLoggerManager::WebXrLoggerManager() = default;

WebXrLoggerManager::~WebXrLoggerManager() = default;

void WebXrLoggerManager::RecordSessionRequest(
    webxr::mojom::SessionRequestRecordPtr session_request_record) {
  for (const auto& remote : remote_set_) {
    remote->AddXrSessionRequest(session_request_record->Clone());
  }

  session_request_records_.push_back(std::move(session_request_record));
}

void WebXrLoggerManager::SubscribeToEvents(
    mojo::PendingRemote<webxr::mojom::XRInternalsSessionListener>
        pending_remote) {
  mojo::Remote<webxr::mojom::XRInternalsSessionListener> remote(
      std::move(pending_remote));

  // Send all previously received options to the remote before adding it to the
  // set.
  for (const auto& request_record : session_request_records_) {
    remote->AddXrSessionRequest(request_record->Clone());
  }

  remote_set_.Add(std::move(remote));
}

}  // namespace content
