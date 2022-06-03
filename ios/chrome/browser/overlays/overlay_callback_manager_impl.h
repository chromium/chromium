// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_CALLBACK_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_CALLBACK_MANAGER_IMPL_H_

#include <vector>

#include "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#include "ios/chrome/browser/overlays/public/overlay_response.h"

// Implementation of OverlayCallbackManager.
class OverlayCallbackManagerImpl : public OverlayCallbackManager {
 public:
  OverlayCallbackManagerImpl();
  ~OverlayCallbackManagerImpl() override;
  OverlayCallbackManagerImpl(const OverlayCallbackManagerImpl&) = delete;

  // Executes the completion callbacks.
  void ExecuteCompletionCallbacks();

  // OverlayCallbackManager:
  void SetCompletionResponse(
      std::unique_ptr<OverlayResponse> response) override;
  OverlayResponse* GetCompletionResponse() const override;
  void AddCompletionCallback(OverlayCompletionCallback callback) override;
  void DispatchResponse(std::unique_ptr<OverlayResponse> response) override;
  void AddDispatchCallback(OverlayDispatchCallback callback) override;

 private:
  std::unique_ptr<OverlayResponse> completion_response_;
  std::vector<OverlayCompletionCallback> completion_callbacks_;
  std::vector<OverlayDispatchCallback> dispatch_callbacks_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_OVERLAY_CALLBACK_MANAGER_IMPL_H_
