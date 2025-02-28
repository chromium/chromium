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

// A boolean value that gets cleared when moved.
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

// Not to be confused with state stored by sites (e.g. cookies, local storage),
// BtmState represents the state recorded by BtmService itself.
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

  TimestampRange site_storage_times() const {
    return state_.site_storage_times;
  }
  TimestampRange user_activation_times() const {
    return state_.user_activation_times;
  }
  TimestampRange stateful_bounce_times() const {
    return state_.stateful_bounce_times;
  }
  TimestampRange bounce_times() const { return state_.bounce_times; }
  TimestampRange web_authn_assertion_times() const {
    return state_.web_authn_assertion_times;
  }

  void update_site_storage_time(base::Time time);
  void update_user_activation_time(base::Time time);
  void update_stateful_bounce_time(base::Time time);
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
