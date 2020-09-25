// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCHEDULING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCHEDULING_H_

#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class IsInputPendingOptions;

// Low-level scheduling primitives for JS scheduler implementations.
class Scheduling : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  bool isInputPending(ScriptState*, const IsInputPendingOptions* options) const;
  bool isFramePending() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCHEDULING_H_
