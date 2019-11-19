// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/idle/idle_state.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

IdleState::IdleState(mojom::blink::IdleStatePtr state)
    : state_(std::move(state)) {}

IdleState::~IdleState() = default;

const mojom::blink::IdleState IdleState::state() const {
  return *state_.get();
}

String IdleState::user() const {
  switch (state_->user) {
    case mojom::blink::UserIdleState::kActive:
      return "active";
    case mojom::blink::UserIdleState::kIdle:
      return "idle";
  }
}

String IdleState::screen() const {
  switch (state_->screen) {
    case mojom::blink::ScreenIdleState::kLocked:
      return "locked";
    case mojom::blink::ScreenIdleState::kUnlocked:
      return "unlocked";
  }
}

}  // namespace blink
