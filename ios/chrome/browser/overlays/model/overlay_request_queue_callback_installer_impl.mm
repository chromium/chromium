// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/overlay_request_queue_callback_installer_impl.h"

#import "base/check.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_callback_installer.h"

#pragma mark - OverlayRequestQueueCallbackInstaller

// static
std::unique_ptr<OverlayRequestQueueCallbackInstaller>
OverlayRequestQueueCallbackInstaller::Create(web::WebState* web_state,
                                             OverlayModality modality) {
  return std::make_unique<OverlayRequestQueueCallbackInstallerImpl>(web_state,
                                                                    modality);
}

#pragma mark - OverlayRequestQueueCallbackInstallerImpl

OverlayRequestQueueCallbackInstallerImpl::
    OverlayRequestQueueCallbackInstallerImpl(web::WebState* web_state,
                                             OverlayModality modality)
    : request_added_observer_(web_state, modality) {}

OverlayRequestQueueCallbackInstallerImpl::
    ~OverlayRequestQueueCallbackInstallerImpl() = default;

#pragma mark OverlayRequestQueueCallbackInstaller

void OverlayRequestQueueCallbackInstallerImpl::AddRequestCallbackInstaller(
    std::unique_ptr<OverlayRequestCallbackInstaller> installer) {
  request_added_observer_.AddInstaller(std::move(installer));
}

#pragma mark - OverlayRequestQueueCallbackInstallerImpl::RequestAddedObserver

OverlayRequestQueueCallbackInstallerImpl::RequestAddedObserver::
    RequestAddedObserver(web::WebState* web_state, OverlayModality modality) {
  DCHECK(web_state);
  scoped_observation_.Observe(
      OverlayRequestQueueImpl::FromWebState(web_state, modality));
}

OverlayRequestQueueCallbackInstallerImpl::RequestAddedObserver::
    ~RequestAddedObserver() = default;

#pragma mark Public

void OverlayRequestQueueCallbackInstallerImpl::RequestAddedObserver::
    AddInstaller(std::unique_ptr<OverlayRequestCallbackInstaller> installer) {
  DCHECK(installer);
  installers_.push_back(std::move(installer));
}

#pragma mark OverlayRequestQueueImpl::Observer

void OverlayRequestQueueCallbackInstallerImpl::RequestAddedObserver::
    RequestAddedToQueue(OverlayRequestQueueImpl* queue,
                        OverlayRequest* request,
                        size_t index) {
  for (auto& installer : installers_) {
    installer->InstallCallbacks(request);
  }
}

void OverlayRequestQueueCallbackInstallerImpl::RequestAddedObserver::
    OverlayRequestQueueDestroyed(OverlayRequestQueueImpl* queue) {
  DCHECK(scoped_observation_.IsObservingSource(queue));
  scoped_observation_.Reset();
}
