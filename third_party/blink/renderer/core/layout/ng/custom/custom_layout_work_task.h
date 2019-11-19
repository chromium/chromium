// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_CUSTOM_LAYOUT_WORK_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_CUSTOM_LAYOUT_WORK_TASK_H_

#include "third_party/blink/renderer/core/layout/ng/custom/custom_layout_constraints_options.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ComputedStyle;
class CustomLayoutChild;
class CustomLayoutToken;
class NGConstraintSpace;
class SerializedScriptValue;
class ScriptPromiseResolver;

// Contains all the information needed to resolve a promise with a fragment or
// intrinsic-sizes.
class CustomLayoutWorkTask {
 public:
  CustomLayoutWorkTask(CustomLayoutChild*,
                       CustomLayoutToken*,
                       ScriptPromiseResolver*,
                       const CustomLayoutConstraintsOptions*,
                       scoped_refptr<SerializedScriptValue> constraint_data);
  ~CustomLayoutWorkTask();

  // Runs this work task.
  void Run(const NGConstraintSpace& parent_space,
           const ComputedStyle& parent_style);

 private:
  Persistent<CustomLayoutChild> child_;
  Persistent<CustomLayoutToken> token_;
  Persistent<ScriptPromiseResolver> resolver_;
  Persistent<const CustomLayoutConstraintsOptions> options_;
  scoped_refptr<SerializedScriptValue> constraint_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_CUSTOM_CUSTOM_LAYOUT_WORK_TASK_H_
