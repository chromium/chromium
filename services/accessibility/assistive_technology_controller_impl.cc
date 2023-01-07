// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/assistive_technology_controller_impl.h"

namespace ax {

AssistiveTechnologyControllerImpl::AssistiveTechnologyControllerImpl() =
    default;

AssistiveTechnologyControllerImpl::~AssistiveTechnologyControllerImpl() =
    default;

void AssistiveTechnologyControllerImpl::Bind(
    mojo::PendingReceiver<mojom::AssistiveTechnologyController>
        at_controller_receiver) {
  at_controller_receivers_.Add(this, std::move(at_controller_receiver));
}

void AssistiveTechnologyControllerImpl::EnableAssistiveTechnology(
    mojom::AssistiveTechnologyType type,
    bool enabled) {
  if (enabled) {
    enabled_ATs_.insert(type);
  } else {
    enabled_ATs_.erase(type);
  }
  // TODO(crbug.com/1355633): Load or unload features from V8.
  // Turn on/off V8 if enabled_ATs_ size changed between 0 and non-zero.
}

bool AssistiveTechnologyControllerImpl::IsFeatureEnabled(
    mojom::AssistiveTechnologyType type) const {
  return enabled_ATs_.find(type) != enabled_ATs_.end();
}

}  // namespace ax
