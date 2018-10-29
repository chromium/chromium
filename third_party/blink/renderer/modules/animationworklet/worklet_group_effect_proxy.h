// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_WORKLET_GROUP_EFFECT_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_WORKLET_GROUP_EFFECT_PROXY_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/modules/animationworklet/effect_proxy.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class MODULES_EXPORT WorkletGroupEffectProxy : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit WorkletGroupEffectProxy(int num_effects);
  HeapVector<Member<EffectProxy>>& getChildren() { return effects_; }
  void Trace(blink::Visitor*) override;

 private:
  HeapVector<Member<EffectProxy>> effects_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ANIMATIONWORKLET_WORKLET_GROUP_EFFECT_PROXY_H_
