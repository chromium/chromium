// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/client_change_tracker.h"

#include "services/ws/client_change.h"

namespace ws {

ClientChangeTracker::ClientChangeTracker() = default;

ClientChangeTracker::~ClientChangeTracker() = default;

bool ClientChangeTracker::IsProcessingChangeForWindow(
    aura::Window* window,
    ClientChangeType type) const {
  return DoesCurrentChangeEqual(window, type, nullptr);
}

bool ClientChangeTracker::IsProcessingPropertyChangeForWindow(
    aura::Window* window,
    const void* property_key) const {
  return DoesCurrentChangeEqual(window, ClientChangeType::kProperty,
                                property_key);
}

bool ClientChangeTracker::DoesCurrentChangeEqual(
    aura::Window* window,
    ClientChangeType type,
    const void* property_key) const {
  return current_change_ && current_change_->window() == window &&
         current_change_->type() == type &&
         current_change_->property_key() == property_key;
}

}  // namespace ws
