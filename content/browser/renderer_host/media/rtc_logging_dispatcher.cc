// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/rtc_logging_dispatcher.h"

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/uuid.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/rtc_logging/rtc_logging.mojom.h"

namespace content {

RTCLoggingDispatcherImpl::RTCLoggingDispatcherImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::RTCLoggingDispatcher> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

RTCLoggingDispatcherImpl::~RTCLoggingDispatcherImpl() = default;

void RTCLoggingDispatcherImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::RTCLoggingDispatcher> receiver) {
  CHECK(render_frame_host);
  new RTCLoggingDispatcherImpl(*render_frame_host, std::move(receiver));
}

void RTCLoggingDispatcherImpl::StartDiagnosticLogging(
    bool upload,
    const base::flat_map<std::string, std::string>& metadata,
    StartDiagnosticLoggingCallback callback) {
  if (!base::FeatureList::IsEnabled(blink::features::kRTCDiagnosticLogging)) {
    ReportBadMessageAndDeleteThis("RTCDiagnosticLogging feature not enabled");
    return;
  }

  // It is expected that the renderer validates the metadata and sends only
  // valid input.
  if (metadata.size() > kMaxMetadataSize) {
    ReportBadMessageAndDeleteThis("Too many metadata entries");
    return;
  }
  for (const auto& [key, value] : metadata) {
    if (key.length() > kMaxMetadataLength ||
        value.length() > kMaxMetadataLength) {
      ReportBadMessageAndDeleteThis("Metadata key or value too long");
      return;
    }
  }

  GetContentClient()->browser()->StartRtcDiagnosticLogging(
      render_frame_host(), upload, metadata, std::move(callback));
}

void RTCLoggingDispatcherImpl::FinishDiagnosticLogging(
    FinishDiagnosticLoggingCallback callback) {
  if (!base::FeatureList::IsEnabled(blink::features::kRTCDiagnosticLogging)) {
    ReportBadMessageAndDeleteThis("RTCDiagnosticLogging feature not enabled");
    return;
  }

  GetContentClient()->browser()->FinishRtcDiagnosticLogging(
      render_frame_host(), {}, std::move(callback));
}

void RTCLoggingDispatcherImpl::CancelDiagnosticLogging(
    CancelDiagnosticLoggingCallback callback) {
  if (!base::FeatureList::IsEnabled(blink::features::kRTCDiagnosticLogging)) {
    ReportBadMessageAndDeleteThis("RTCDiagnosticLogging feature not enabled");
    return;
  }

  GetContentClient()->browser()->CancelRtcDiagnosticLogging(
      render_frame_host(), std::move(callback));
}

}  // namespace content
