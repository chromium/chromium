// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_state.h"

#include "content/browser/btm/btm_storage.h"
#include "content/browser/btm/btm_utils.h"

namespace content {

BtmState::BtmState(BtmStorage* storage, std::string site)
    : storage_(storage), site_(std::move(site)), was_loaded_(false) {}

BtmState::BtmState(BtmStorage* storage,
                   std::string site,
                   const StateValue& state)
    : storage_(storage),
      site_(std::move(site)),
      was_loaded_(true),
      state_(state) {}

BtmState::BtmState(BtmState&&) = default;
BtmState& BtmState::operator=(BtmState&&) = default;

BtmState::~BtmState() {
  if (dirty_) {
    storage_->Write(*this);
  }
}


void BtmState::update_user_activation_time(base::Time time) {
  dirty_ |= UpdateTimestampRange(state_.user_activation_times, time);
}

void BtmState::update_bounce_time(base::Time time) {
  dirty_ |= UpdateTimestampRange(state_.bounce_times, time);
}

void BtmState::update_web_authn_assertion_time(base::Time time) {
  dirty_ |= UpdateTimestampRange(state_.web_authn_assertion_times, time);
}

}  // namespace content
