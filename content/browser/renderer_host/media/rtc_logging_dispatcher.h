// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_RTC_LOGGING_DISPATCHER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_RTC_LOGGING_DISPATCHER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/rtc_logging/rtc_logging.mojom.h"

namespace content {

class CONTENT_EXPORT RTCLoggingDispatcherImpl
    : public DocumentService<blink::mojom::RTCLoggingDispatcher> {
 public:
  static constexpr size_t kMaxMetadataSize = 5;
  static constexpr size_t kMaxMetadataLength = 100;

  RTCLoggingDispatcherImpl(const RTCLoggingDispatcherImpl&) = delete;
  RTCLoggingDispatcherImpl& operator=(const RTCLoggingDispatcherImpl&) = delete;

  ~RTCLoggingDispatcherImpl() override;

  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::RTCLoggingDispatcher> receiver);

  // blink::mojom::RTCLoggingDispatcher
  void StartDiagnosticLogging(
      bool upload,
      const base::flat_map<std::string, std::string>& metadata,
      StartDiagnosticLoggingCallback callback) override;
  void FinishDiagnosticLogging(
      FinishDiagnosticLoggingCallback callback) override;
  void CancelDiagnosticLogging(
      CancelDiagnosticLoggingCallback callback) override;

 private:
  RTCLoggingDispatcherImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::RTCLoggingDispatcher> receiver);

  base::WeakPtrFactory<RTCLoggingDispatcherImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_RTC_LOGGING_DISPATCHER_H_
