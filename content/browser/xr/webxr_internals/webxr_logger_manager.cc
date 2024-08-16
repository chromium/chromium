// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/webxr_internals/webxr_logger_manager.h"

#include "base/time/time.h"
#include "content/browser/xr/webxr_internals/mojom/webxr_internals.mojom.h"
#include "device/vr/public/mojom/xr_device.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {

WebXrLoggerManager::WebXrLoggerManager() = default;

WebXrLoggerManager::~WebXrLoggerManager() = default;

void WebXrLoggerManager::RecordSessionRequested(
    webxr::mojom::SessionRequestedRecordPtr session_requested_record) {
  for (const auto& remote : remote_set_) {
    remote->LogXrSessionRequested(session_requested_record->Clone());
  }

  session_requested_records_.push_back(std::move(session_requested_record));
}

void WebXrLoggerManager::RecordSessionRejected(
    webxr::mojom::SessionRejectedRecordPtr session_rejected_record) {
  for (const auto& remote : remote_set_) {
    remote->LogXrSessionRejected(session_rejected_record->Clone());
  }

  session_rejected_records_.push_back(std::move(session_rejected_record));
}

void WebXrLoggerManager::RecordSessionStarted(
    webxr::mojom::SessionStartedRecordPtr session_started_record) {
  for (const auto& remote : remote_set_) {
    remote->LogXrSessionStarted(session_started_record->Clone());
  }

  session_started_records_.push_back(std::move(session_started_record));
}

void WebXrLoggerManager::RecordSessionStopped(
    webxr::mojom::SessionStoppedRecordPtr session_stopped_record) {
  for (const auto& remote : remote_set_) {
    remote->LogXrSessionStopped(session_stopped_record->Clone());
  }

  session_stopped_records_.push_back(std::move(session_stopped_record));
  renderer_listener_receiver_.reset();
}

void WebXrLoggerManager::RecordRuntimeAdded(
    webxr::mojom::RuntimeInfoPtr runtime_added_record) {
  for (const auto& remote : remote_set_) {
    remote->LogXrRuntimeAdded(runtime_added_record->Clone());
  }
}

void WebXrLoggerManager::RecordRuntimeRemoved(
    device::mojom::XRDeviceId device_id) {
  for (const auto& remote : remote_set_) {
    remote->LogXrRuntimeRemoved(device_id);
  }
}

void WebXrLoggerManager::SubscribeToEvents(
    mojo::PendingRemote<webxr::mojom::XRInternalsSessionListener>
        pending_remote) {
  mojo::Remote<webxr::mojom::XRInternalsSessionListener> remote(
      std::move(pending_remote));

  // Send all previously received options to the remote before adding it to the
  // set.
  for (const auto& requested_record : session_requested_records_) {
    remote->LogXrSessionRequested(requested_record->Clone());
  }

  for (const auto& rejected_record : session_rejected_records_) {
    remote->LogXrSessionRejected(rejected_record->Clone());
  }

  for (const auto& started_record : session_started_records_) {
    remote->LogXrSessionStarted(started_record->Clone());
  }

  for (const auto& stopped_record : session_stopped_records_) {
    remote->LogXrSessionStopped(stopped_record->Clone());
  }

  remote_set_.Add(std::move(remote));
}

mojo::PendingRemote<device::mojom::WebXrInternalsRendererListener>
WebXrLoggerManager::BindRenderListener() {
  return renderer_listener_receiver_.BindNewPipeAndPassRemote();
}

void WebXrLoggerManager::OnFrameData(
    device::mojom::XrFrameStatisticsPtr xrframe_statistics) {
  for (const auto& remote : remote_set_) {
    remote->LogFrameData(xrframe_statistics->Clone());
  }
}

void WebXrLoggerManager::OnConsoleLog(
    device::mojom::XrLogMessagePtr xr_logging_statistics) {
  for (const auto& remote : remote_set_) {
    remote->LogConsoleMessages(xr_logging_statistics->Clone());
  }
}
}  // namespace content
