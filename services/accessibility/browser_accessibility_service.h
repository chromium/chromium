// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_BROWSER_ACCESSIBILITY_SERVICE_H_
#define SERVICES_ACCESSIBILITY_BROWSER_ACCESSIBILITY_SERVICE_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {
class AutomationImpl;

// Implementation of the Accessibility Service for Chrome.
class BrowserAccessibilityService : public mojom::AccessibilityService {
 public:
  explicit BrowserAccessibilityService(
      mojo::PendingReceiver<mojom::AccessibilityService> receiver);
  ~BrowserAccessibilityService() override;
  BrowserAccessibilityService(const BrowserAccessibilityService&) = delete;
  BrowserAccessibilityService& operator=(const BrowserAccessibilityService&) =
      delete;

 private:
  // mojom::AccessibilityService:
  void BindAccessibilityServiceClient(
      mojo::PendingRemote<mojom::AccessibilityServiceClient>
          accessibility_client_remote) override;

  std::unique_ptr<AutomationImpl> automation_;

  mojo::Receiver<mojom::AccessibilityService> receiver_;
  mojo::Remote<mojom::AccessibilityServiceClient>
      accessibility_service_client_remote_;

  base::WeakPtrFactory<BrowserAccessibilityService> weak_ptr_factory_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_BROWSER_ACCESSIBILITY_SERVICE_H_
