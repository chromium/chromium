// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INITIAL_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INITIAL_DATA_H_

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/style/style_variables.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class PropertyRegistry;

// Holds data stored on the initial ComputedStyle only.
//
// An instance of this class is created once, and then shared between all the
// ComputedStyles that inherit (directly or indirectly) from the initial style.
class CORE_EXPORT StyleInitialData : public RefCounted<StyleInitialData> {
 public:
  static scoped_refptr<StyleInitialData> Create(
      const PropertyRegistry& registry) {
    return base::AdoptRef(new StyleInitialData(registry));
  }

  bool operator==(const StyleInitialData& other) const;
  bool operator!=(const StyleInitialData& other) const {
    return !(*this == other);
  }

  bool HasInitialVariables() const { return !variables_.IsEmpty(); }

  CSSVariableData* GetVariableData(const AtomicString& name) const {
    return variables_.GetData(name).value_or(nullptr);
  }

  const CSSValue* GetVariableValue(const AtomicString& name) const {
    return variables_.GetValue(name).value_or(nullptr);
  }

 private:
  StyleInitialData(const PropertyRegistry&);

  // Initial values for all registered properties. This is set on
  // the initial style, and then shared with all other styles that directly or
  // indirectly inherit from that.
  StyleVariables variables_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_INITIAL_DATA_H_
