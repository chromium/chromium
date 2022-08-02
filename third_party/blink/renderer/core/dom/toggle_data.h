// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TOGGLE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TOGGLE_DATA_H_

#include "third_party/blink/renderer/core/dom/toggle.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

// Represents the set of toggles on an element.
using ToggleMap = WTF::HashMap<AtomicString, Toggle>;

class ToggleData {
 public:
  ToggleMap& Toggles() { return toggles_; }

  // TODO(dbaron): Add API for toggle group information here.

 private:
  ToggleMap toggles_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_TOGGLE_DATA_H_
