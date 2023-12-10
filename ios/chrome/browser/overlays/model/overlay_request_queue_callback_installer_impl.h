// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_REQUEST_QUEUE_CALLBACK_INSTALLER_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_REQUEST_QUEUE_CALLBACK_INSTALLER_IMPL_H_

#include <vector>

#import "ios/chrome/browser/overlays/model/public/overlay_request_queue_callback_installer.h"

#include "base/scoped_observation.h"
#import "ios/chrome/browser/overlays/model/overlay_request_queue_impl.h"

class OverlayRequestQueueCallbackInstallerImpl
    : public OverlayRequestQueueCallbackInstaller {
 public:
  OverlayRequestQueueCallbackInstallerImpl(web::WebState* web_state,
                                           OverlayModality modality);
  ~OverlayRequestQueueCallbackInstallerImpl() override;

 private:
  // Observer that installs callbacks for each request added to a queue.
  class RequestAddedObserver : public OverlayRequestQueueImpl::Observer {
   public:
    RequestAddedObserver(web::WebState* web_state, OverlayModality modality);
    ~RequestAddedObserver() override;

    // Adds `installer` to be executed for every request added to the queue.
    void AddInstaller(
        std::unique_ptr<OverlayRequestCallbackInstaller> installer);

   private:
    // OverlayRequestQueueImpl::Observer:
    void RequestAddedToQueue(OverlayRequestQueueImpl* queue,
                             OverlayRequest* request,
                             size_t index) override;
    void OverlayRequestQueueDestroyed(OverlayRequestQueueImpl* queue) override;

    // The installers for the queue.
    std::vector<std::unique_ptr<OverlayRequestCallbackInstaller>> installers_;
    base::ScopedObservation<OverlayRequestQueueImpl,
                            OverlayRequestQueueImpl::Observer>
        scoped_observation_{this};
  };

  // OverlayRequestQueueCallbackInstaller:
  void AddRequestCallbackInstaller(
      std::unique_ptr<OverlayRequestCallbackInstaller> installer) override;

  // The observer responsible for installing callbacks for each OverlayRequest
  // added to a queue.
  RequestAddedObserver request_added_observer_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_OVERLAY_REQUEST_QUEUE_CALLBACK_INSTALLER_IMPL_H_
