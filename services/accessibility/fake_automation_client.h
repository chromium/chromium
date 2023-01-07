// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FAKE_AUTOMATION_CLIENT_H_
#define SERVICES_ACCESSIBILITY_FAKE_AUTOMATION_CLIENT_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"

namespace ax {

// A fake Automation Client for use in tests.
// TODO(crbug.com/1355633) This can be extended to allow for passing events into
// the service once the mojom is landed.
class FakeAutomationClient : public mojom::AutomationClient {
 public:
  explicit FakeAutomationClient(mojom::AccessibilityService* service);
  FakeAutomationClient(const FakeAutomationClient& other) = delete;
  FakeAutomationClient& operator=(const FakeAutomationClient&) = delete;
  ~FakeAutomationClient() override;

  // Methods for testing.
  void BindToAutomation();
  bool IsBound();

 private:
  mojom::AccessibilityService* service_;

  mojo::Remote<mojom::Automation> automation_;
  mojo::Receiver<mojom::AutomationClient> automation_client_receiver_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FAKE_AUTOMATION_CLIENT_H_
