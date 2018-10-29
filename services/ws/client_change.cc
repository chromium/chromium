// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/client_change.h"

#include <utility>

#include "services/ws/client_change_tracker.h"
#include "ui/aura/window.h"

namespace ws {

ClientChange::ClientChange(ClientChangeTracker* tracker,
                           aura::Window* window,
                           ClientChangeType type,
                           const void* property_key)
    : tracker_(tracker), type_(type), property_key_(property_key) {
  DCHECK(!tracker_->current_change_);
  tracker_->current_change_ = this;
  if (window)
    window_tracker_.Add(window);
}

ClientChange::~ClientChange() {
  DCHECK_EQ(this, tracker_->current_change_);
  tracker_->current_change_ = nullptr;
}

}  // namespace ws
