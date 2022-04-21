// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_ROOT_H_

#include "third_party/blink/renderer/core/style/toggle_group.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class ToggleRoot {
  DISALLOW_NEW();

 public:
  ToggleRoot(const AtomicString& name,
             uint32_t initial_state,
             uint32_t maximum_state,
             bool is_sticky,
             bool is_group,
             ToggleScope scope)
      : name_(name),
        initial_state_(initial_state),
        maximum_state_(maximum_state),
        is_sticky_(is_sticky),
        is_group_(is_group),
        scope_(scope) {
    DCHECK_EQ(scope_, scope) << "sufficient field width";
  }

  ToggleRoot(const ToggleRoot&) = default;
  ~ToggleRoot() = default;

  bool operator==(const ToggleRoot& other) const {
    return name_ == other.name_ && initial_state_ == other.initial_state_ &&
           maximum_state_ == other.maximum_state_ &&
           is_sticky_ == other.is_sticky_ && is_group_ == other.is_group_ &&
           scope_ == other.scope_;
  }
  bool operator!=(const ToggleRoot& other) const { return !(*this == other); }

  const AtomicString& Name() const { return name_; }
  uint32_t InitialState() const { return initial_state_; }
  uint32_t MaximumState() const { return maximum_state_; }
  bool IsSticky() const { return is_sticky_; }
  bool IsGroup() const { return is_group_; }
  ToggleScope Scope() const { return scope_; }

 private:
  const AtomicString name_;
  const uint32_t initial_state_;
  const uint32_t maximum_state_;
  const bool is_sticky_ : 1;
  const bool is_group_ : 1;
  const ToggleScope scope_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_ROOT_H_
