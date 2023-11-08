// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/model/public/overlay_request_callback_installer.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_support.h"

OverlayRequestCallbackInstaller::OverlayRequestCallbackInstaller() = default;

OverlayRequestCallbackInstaller::~OverlayRequestCallbackInstaller() = default;

void OverlayRequestCallbackInstaller::InstallCallbacks(
    OverlayRequest* request) {
  // Early return if `request` is unsupported or if callbacks have already been
  // installed.
  if (!GetRequestSupport()->IsRequestSupported(request) ||
      base::Contains(requests_, request)) {
    return;
  }
  requests_.insert(request);

  // Add the completion callback to remove the request from `requests_`.
  request->GetCallbackManager()->AddCompletionCallback(
      base::BindOnce(&OverlayRequestCallbackInstaller::OverlayCompleted,
                     weak_factory_.GetWeakPtr(), request));

  // Allow subclasses to install their own callbacks.
  InstallCallbacksInternal(request);
}

const OverlayRequestSupport*
OverlayRequestCallbackInstaller::GetRequestSupport() const {
  return OverlayRequestSupport::All();
}

void OverlayRequestCallbackInstaller::InstallCallbacksInternal(
    OverlayRequest* request) {}

void OverlayRequestCallbackInstaller::OverlayCompleted(
    OverlayRequest* request,
    OverlayResponse* response) {
  requests_.erase(request);
}
