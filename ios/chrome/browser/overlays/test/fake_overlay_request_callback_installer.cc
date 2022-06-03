// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/overlays/test/fake_overlay_request_callback_installer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_dispatch_callback.h"
#include "ios/chrome/browser/overlays/public/overlay_response_support.h"

#pragma mark - MockOverlayRequestCallbackReceiver

MockOverlayRequestCallbackReceiver::MockOverlayRequestCallbackReceiver() =
    default;

MockOverlayRequestCallbackReceiver::~MockOverlayRequestCallbackReceiver() =
    default;

#pragma mark - FakeOverlayRequestCallbackReceiver

void FakeOverlayRequestCallbackReceiver::RunCompletionCallback(
    OverlayRequest* request,
    OverlayResponse* response) {
  CompletionCallback(request);
}

void FakeOverlayRequestCallbackReceiver::RunDispatchCallback(
    OverlayRequest* request,
    const OverlayResponseSupport* response_support,
    OverlayResponse* response) {
  DispatchCallback(request, response_support);
}

#pragma mark - FakeOverlayRequestCallbackInstaller

FakeOverlayRequestCallbackInstaller::FakeOverlayRequestCallbackInstaller(
    FakeOverlayRequestCallbackReceiver* receiver,
    const std::set<const OverlayResponseSupport*>& dispatch_supports)
    : receiver_(receiver), dispatch_supports_(dispatch_supports) {
  DCHECK(receiver_);
}

FakeOverlayRequestCallbackInstaller::~FakeOverlayRequestCallbackInstaller() =
    default;

#pragma mark - OverlayRequestCallbackInstaller

const OverlayRequestSupport*
FakeOverlayRequestCallbackInstaller::GetRequestSupport() const {
  return request_support_;
}

void FakeOverlayRequestCallbackInstaller::InstallCallbacksInternal(
    OverlayRequest* request) {
  OverlayCallbackManager* manager = request->GetCallbackManager();
  manager->AddCompletionCallback(
      base::BindOnce(&FakeOverlayRequestCallbackReceiver::RunCompletionCallback,
                     base::Unretained(receiver_), request));
  for (const OverlayResponseSupport* support : dispatch_supports_) {
    manager->AddDispatchCallback(OverlayDispatchCallback(
        base::BindRepeating(
            &FakeOverlayRequestCallbackReceiver::RunDispatchCallback,
            base::Unretained(receiver_), request, support),
        support));
  }
}
