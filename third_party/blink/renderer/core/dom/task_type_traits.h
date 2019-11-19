// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TASK_TYPE_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TASK_TYPE_TRAITS_H_

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

// HashTraits for TaskType.
struct TaskTypeTraits : WTF::GenericHashTraits<TaskType> {
  static const bool kEmptyValueIsZero = false;
  static TaskType EmptyValue() { return static_cast<TaskType>(-1); }
  static void ConstructDeletedValue(TaskType& slot, bool) {
    slot = static_cast<TaskType>(-2);
  }
  static bool IsDeletedValue(TaskType value) {
    return value == static_cast<TaskType>(-2);
  }
};

}  // namespace blink

#endif
