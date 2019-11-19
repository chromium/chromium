// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_TESTING_INTERNALS_ACCESSIBILITY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_TESTING_INTERNALS_ACCESSIBILITY_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Internals;

class InternalsAccessibility {
  STATIC_ONLY(InternalsAccessibility);

 public:
  static unsigned numberOfLiveAXObjects(Internals&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_TESTING_INTERNALS_ACCESSIBILITY_H_
