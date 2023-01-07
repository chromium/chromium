// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_test_utils.h"

namespace content {

MockPresentationServiceDelegate::MockPresentationServiceDelegate() = default;
MockPresentationServiceDelegate::~MockPresentationServiceDelegate() = default;

bool MockPresentationServiceDelegate::AddScreenAvailabilityListener(
    int render_process_id,
    int routing_id,
    PresentationScreenAvailabilityListener* listener) {
  if (!screen_availability_listening_supported_) {
    listener->OnScreenAvailabilityChanged(ScreenAvailability::DISABLED);
  }

  return AddScreenAvailabilityListener();
}

MockPresentationReceiver::MockPresentationReceiver() = default;
MockPresentationReceiver::~MockPresentationReceiver() = default;

MockReceiverPresentationServiceDelegate::
    MockReceiverPresentationServiceDelegate() = default;
MockReceiverPresentationServiceDelegate::
    ~MockReceiverPresentationServiceDelegate() = default;

MockPresentationConnection::MockPresentationConnection() = default;
MockPresentationConnection::~MockPresentationConnection() = default;

MockPresentationController::MockPresentationController() = default;
MockPresentationController::~MockPresentationController() = default;

}  // namespace content
