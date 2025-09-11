// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BTM_BTM_STATE_H_
#define CONTENT_BROWSER_BTM_BTM_STATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/browser/btm/btm_utils.h"
#include "content/common/content_export.h"

namespace content {

class BtmStorage;

// A boolean-like class that tracks whether the `BtmState` has been modified
// since it was loaded from storage.
class DirtyBit {
 public:
  explicit DirtyBit(bool value = false) : value_(value) {}
  DirtyBit(DirtyBit&& old) { *this = std::move(old); }
  DirtyBit& operator=(DirtyBit&& old) {
    value_ = std::exchange(old.value_, false);
    return *this;
  }

  explicit operator bool() const { return value_; }

  DirtyBit& operator|=(bool value) {
    value_ |= value;
    return *this;
  }

  DirtyBit& operator=(bool value) {
    value_ = value;
    return *this;
  }

 private:
  bool value_;
};

// Represents the state recorded by the BTM feature for a particular site. Not
// to be confused with the state stored by the site itself (e.g., cookies, local
// storage). `BtmState` is stored in the BTM database.
class CONTENT_EXPORT BtmState {
 public:
  BtmState(BtmStorage* storage, std::string site);
  // For loaded BtmState.
  BtmState(BtmStorage* storage, std::string site, const StateValue& state);

  BtmState(BtmState&&);
  BtmState& operator=(BtmState&&);
  // Flushes changes to storage_.
  ~BtmState();

  const std::string& site() const { return site_; }
  // True iff this BtmState was loaded from BtmStorage (as opposed to being
  // default-initialized for a new site).
  bool was_loaded() const { return was_loaded_; }

  TimestampRange user_activation_times() const {
    return state_.user_activation_times;
  }
  // The time range of all bounces (both stateful and stateless).
  TimestampRange bounce_times() const { return state_.bounce_times; }
  // The time range of successful WebAuthn assertions (failed WAAs do not
  // count).
  TimestampRange web_authn_assertion_times() const {
    return state_.web_authn_assertion_times;
  }

  void update_user_activation_time(base::Time time);
  void update_bounce_time(base::Time time);
  void update_web_authn_assertion_time(base::Time time);
  StateValue ToStateValue() const { return state_; }

 private:
  raw_ptr<BtmStorage, AcrossTasksDanglingUntriaged> storage_;
  std::string site_;
  bool was_loaded_;
  DirtyBit dirty_;
  StateValue state_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BTM_BTM_STATE_H_
