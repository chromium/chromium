// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_TRIGGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_TRIGGER_H_

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

enum class ToggleTriggerMode : uint8_t { kAdd = 0, kSet = 1 };

class ToggleTrigger {
  DISALLOW_NEW();

 public:
  ToggleTrigger(const AtomicString& name,
                ToggleTriggerMode mode,
                uint32_t value)
      : name_(name), mode_(mode), value_(value) {}

  ToggleTrigger(const ToggleTrigger&) = default;
  ~ToggleTrigger() = default;

  bool operator==(const ToggleTrigger& other) const {
    return name_ == other.name_ && mode_ == other.mode_ &&
           value_ == other.value_;
  }
  bool operator!=(const ToggleTrigger& other) const {
    return !(*this == other);
  }

  const AtomicString& Name() const { return name_; }
  ToggleTriggerMode Mode() const { return mode_; }
  uint32_t Value() const { return value_; }

 private:
  const AtomicString name_;
  const ToggleTriggerMode mode_;
  const uint32_t value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_TRIGGER_H_
