// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_GROUP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_GROUP_H_

#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

enum class ToggleScope : uint8_t { kWide = 0, kNarrow = 1 };

class ToggleGroup {
  DISALLOW_NEW();

 public:
  ToggleGroup(const AtomicString& name, ToggleScope scope)
      : name_(name), scope_(scope) {}

  ToggleGroup(const ToggleGroup&) = default;
  ~ToggleGroup() = default;

  bool operator==(const ToggleGroup& other) const {
    return name_ == other.name_ && scope_ == other.scope_;
  }
  bool operator!=(const ToggleGroup& other) const { return !(*this == other); }

  const AtomicString& Name() const { return name_; }
  ToggleScope Scope() const { return scope_; }

 private:
  const AtomicString name_;
  const ToggleScope scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_GROUP_H_
