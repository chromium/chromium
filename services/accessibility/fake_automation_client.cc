// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/fake_automation_client.h"

namespace ax {
FakeAutomationClient::FakeAutomationClient(mojom::AccessibilityService* service)
    : service_(service) {}

FakeAutomationClient::~FakeAutomationClient() = default;

void FakeAutomationClient::BindToAutomation() {
  service_->BindAutomation(
      automation_client_receiver_.BindNewPipeAndPassRemote(),
      automation_.BindNewPipeAndPassReceiver());
}

bool FakeAutomationClient::IsBound() {
  return automation_.is_bound() && automation_client_receiver_.is_bound();
}

}  // namespace ax
