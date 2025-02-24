// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INTEREST_INVOKER_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INTEREST_INVOKER_DATA_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/html/closewatcher/close_watcher.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// The InterestInvokerData class stores information that is needed when
// the Element it is attached to is an interest invoker (by having the
// `interesttarget` attribute).
class InterestInvokerData final : public GarbageCollected<InterestInvokerData>,
                                  public ElementRareDataField {
 public:
  InterestInvokerData() = default;
  InterestInvokerData(const InterestInvokerData&) = delete;
  InterestInvokerData& operator=(const InterestInvokerData&) = delete;

  bool hasInterestGainedTask() const {
    return interest_gained_task_.IsActive();
  }
  void cancelInterestGainedTask() { interest_gained_task_.Cancel(); }
  void setInterestGainedTask(TaskHandle&& task) {
    DCHECK(RuntimeEnabledFeatures::
               HTMLInterestTargetAttributeEnabledByRuntimeFlag());
    DCHECK(!interest_gained_task_.IsActive());
    interest_gained_task_ = std::move(task);
  }

  bool hasInterestLostTask() const { return interest_lost_task_.IsActive(); }
  void cancelInterestLostTask() { interest_lost_task_.Cancel(); }
  void setInterestLostTask(TaskHandle&& task) {
    DCHECK(RuntimeEnabledFeatures::
               HTMLInterestTargetAttributeEnabledByRuntimeFlag());
    DCHECK(!interest_lost_task_.IsActive());
    interest_lost_task_ = std::move(task);
  }

  void Trace(Visitor* visitor) const override {
    ElementRareDataField::Trace(visitor);
  }

 private:
  TaskHandle interest_gained_task_;
  TaskHandle interest_lost_task_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INTEREST_INVOKER_DATA_H_
