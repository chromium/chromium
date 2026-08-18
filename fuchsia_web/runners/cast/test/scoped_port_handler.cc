// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/test/scoped_port_handler.h"

#include <memory>
#include <string_view>
#include <utility>

ScopedPortHandler::ScopedPortHandler(cast_api_bindings::Manager& manager,
                                     std::string_view port_name)
    : manager_(manager), port_name_(port_name) {
  manager_->RegisterPortHandler(port_name_,
                                future_port_.GetRepeatingCallback());
}

ScopedPortHandler::~ScopedPortHandler() {
  manager_->UnregisterPortHandler(port_name_);
}

std::unique_ptr<cast_api_bindings::MessagePort>
ScopedPortHandler::RunUntilPortConnected() {
  return future_port_.Take();
}
