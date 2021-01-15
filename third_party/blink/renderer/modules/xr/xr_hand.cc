// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hand.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_joint_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_joint_space.h"

namespace blink {

unsigned int XRHand::size() const {
  NOTIMPLEMENTED();
  return 0;
}

XRJointSpace* XRHand::get(const String& key) {
  NOTIMPLEMENTED();
  return nullptr;
}

XRHand::IterationSource* XRHand::StartIteration(ScriptState*, ExceptionState&) {
  NOTIMPLEMENTED();
  return nullptr;
}

void XRHand::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
