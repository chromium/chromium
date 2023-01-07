// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/document_state.h"

#include "content/renderer/navigation_state.h"

namespace content {

DocumentState::DocumentState() {}

DocumentState::~DocumentState() {}

void DocumentState::set_navigation_state(
    std::unique_ptr<NavigationState> navigation_state) {
  navigation_state_ = std::move(navigation_state);
}

}  // namespace content
