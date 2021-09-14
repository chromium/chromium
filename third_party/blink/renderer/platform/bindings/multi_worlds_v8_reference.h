// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_MULTI_WORLDS_V8_REFERENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_MULTI_WORLDS_V8_REFERENCE_H_

#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class MultiWorldsV8Reference : public GarbageCollected<MultiWorldsV8Reference> {
 public:
  virtual void Trace(Visitor*) const {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_MULTI_WORLDS_V8_REFERENCE_H_
