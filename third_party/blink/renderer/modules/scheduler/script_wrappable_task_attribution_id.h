// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCRIPT_WRAPPABLE_TASK_ATTRIBUTION_ID_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCRIPT_WRAPPABLE_TASK_ATTRIBUTION_ID_H_

#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"

namespace blink {

class ScriptWrappableTaskAttributionId final
    : public ScriptWrappable,
      public scheduler::TaskAttributionId {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit ScriptWrappableTaskAttributionId(
      const scheduler::TaskAttributionId& id)
      : TaskAttributionId(id) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCHEDULER_SCRIPT_WRAPPABLE_TASK_ATTRIBUTION_ID_H_
