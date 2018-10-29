// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/animationworklet/worklet_group_effect_proxy.h"

namespace blink {

WorkletGroupEffectProxy::WorkletGroupEffectProxy(int num_effects)
    : effects_(num_effects) {
  for (int i = 0; i < num_effects; ++i)
    effects_[i] = new EffectProxy();
}

void WorkletGroupEffectProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(effects_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
