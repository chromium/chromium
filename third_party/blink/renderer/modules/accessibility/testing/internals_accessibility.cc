// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/testing/internals_accessibility.h"

#include "third_party/blink/renderer/core/testing/internals.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"

namespace blink {

unsigned InternalsAccessibility::numberOfLiveAXObjects(Internals&) {
  return AXObject::NumberOfLiveAXObjects();
}

}  // namespace blink
