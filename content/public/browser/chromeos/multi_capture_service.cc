// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/chromeos/multi_capture_service.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "content/public/browser/browser_thread.h"

namespace content {

MultiCaptureService::MultiCaptureService() = default;

MultiCaptureService::~MultiCaptureService() = default;

void MultiCaptureService::BindMultiCaptureService(
    mojo::PendingReceiver<video_capture::mojom::MultiCaptureService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  multi_capture_service_receiver_set_.Add(this, std::move(receiver));
}

void MultiCaptureService::AddObserver(
    mojo::PendingRemote<video_capture::mojom::MultiCaptureServiceClient>
        observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.Add(std::move(observer));
}

void MultiCaptureService::NotifyMultiCaptureStarted(const std::string& label,
                                                    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& observer : observers_) {
    observer->MultiCaptureStarted(label, origin);
  }
}

void MultiCaptureService::NotifyMultiCaptureStartedFromApp(
    const std::string& label,
    const std::string& app_id,
    const std::string& app_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const mojo::Remote<video_capture::mojom::MultiCaptureServiceClient>&
           observer : observers_) {
    observer->MultiCaptureStartedFromApp(label, app_id, app_name);
  }
}

void MultiCaptureService::NotifyMultiCaptureStopped(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const mojo::Remote<video_capture::mojom::MultiCaptureServiceClient>&
           observer : observers_) {
    observer->MultiCaptureStopped(label);
  }
}

MultiCaptureService& GetMultiCaptureService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static base::NoDestructor<MultiCaptureService> multi_capture_service;
  return *multi_capture_service;
}

}  // namespace content
