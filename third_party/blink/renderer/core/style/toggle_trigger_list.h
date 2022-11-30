// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_TRIGGER_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_TRIGGER_LIST_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/style/toggle_trigger.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

typedef WTF::Vector<ToggleTrigger, 1> ToggleTriggerVector;

class ToggleTriggerList : public RefCounted<ToggleTriggerList> {
 public:
  static scoped_refptr<ToggleTriggerList> Create() {
    return base::AdoptRef(new ToggleTriggerList());
  }

  bool operator==(const ToggleTriggerList& other) const {
    return triggers_ == other.triggers_;
  }
  bool operator!=(const ToggleTriggerList& other) const {
    return !(*this == other);
  }

  void Append(ToggleTrigger&& trigger) {
    triggers_.push_back(std::move(trigger));
  }

  const ToggleTriggerVector& Triggers() const { return triggers_; }

 private:
  ToggleTriggerVector triggers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_TRIGGER_LIST_H_
