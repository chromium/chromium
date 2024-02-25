// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CHROMEOS_MULTI_CAPTURE_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_CHROMEOS_MULTI_CAPTURE_SERVICE_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/video_capture/public/mojom/multi_capture_service.mojom.h"

namespace content {

class CONTENT_EXPORT MultiCaptureService
    : public video_capture::mojom::MultiCaptureService {
 public:
  MultiCaptureService();
  MultiCaptureService(const MultiCaptureService&) = delete;
  MultiCaptureService& operator=(const MultiCaptureService&) = delete;
  ~MultiCaptureService() override;

  void BindMultiCaptureService(
      mojo::PendingReceiver<video_capture::mojom::MultiCaptureService>
          receiver);

  // video_capture::mojom::MultiCaptureService:
  void AddObserver(
      mojo::PendingRemote<video_capture::mojom::MultiCaptureServiceClient>
          observer) override;

  void NotifyMultiCaptureStarted(const std::string& label,
                                 const url::Origin& origin);
  void NotifyMultiCaptureStartedFromApp(const std::string& label,
                                        const std::string& app_id,
                                        const std::string& app_name);
  void NotifyMultiCaptureStopped(const std::string& label);

 private:
  mojo::ReceiverSet<video_capture::mojom::MultiCaptureService>
      multi_capture_service_receiver_set_;
  mojo::RemoteSet<video_capture::mojom::MultiCaptureServiceClient> observers_;
};

CONTENT_EXPORT MultiCaptureService& GetMultiCaptureService();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CHROMEOS_MULTI_CAPTURE_SERVICE_H_
