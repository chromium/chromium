// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_MAP_H_

#include "third_party/blink/renderer/core/dom/css_toggle.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

// Represents the set of toggles on an element.
using ToggleMap = HeapHashMap<AtomicString, Member<CSSToggle>>;

class CORE_EXPORT CSSToggleMap : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ToggleMap& Toggles() { return toggles_; }

  void Trace(Visitor* visitor) const override;

 private:
  ToggleMap toggles_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CSS_TOGGLE_MAP_H_
