// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_NON_INHERITED_VARIABLES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_NON_INHERITED_VARIABLES_H_

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class CORE_EXPORT StyleNonInheritedVariables {
 public:
  static std::unique_ptr<StyleNonInheritedVariables> Create() {
    return base::WrapUnique(new StyleNonInheritedVariables);
  }

  std::unique_ptr<StyleNonInheritedVariables> Clone() {
    return base::WrapUnique(new StyleNonInheritedVariables(*this));
  }

  bool operator==(const StyleNonInheritedVariables& other) const;
  bool operator!=(const StyleNonInheritedVariables& other) const {
    return !(*this == other);
  }

  void SetVariable(const AtomicString& name,
                   scoped_refptr<CSSVariableData> value) {
    needs_resolution_ =
        needs_resolution_ || (value && (value->NeedsVariableResolution() ||
                                        value->NeedsUrlResolution()));
    data_.Set(name, std::move(value));
  }
  CSSVariableData* GetVariable(const AtomicString& name) const;
  void RemoveVariable(const AtomicString&);

  void SetRegisteredVariable(const AtomicString&, const CSSValue*);
  const CSSValue* RegisteredVariable(const AtomicString& name) const {
    return registered_data_->at(name);
  }

  HashSet<AtomicString> GetCustomPropertyNames() const;

  bool NeedsResolution() const { return needs_resolution_; }
  void ClearNeedsResolution() { needs_resolution_ = false; }

 private:
  StyleNonInheritedVariables();
  StyleNonInheritedVariables(StyleNonInheritedVariables&);

  friend class CSSVariableResolver;

  HashMap<AtomicString, scoped_refptr<CSSVariableData>> data_;
  Persistent<HeapHashMap<AtomicString, Member<CSSValue>>> registered_data_;
  bool needs_resolution_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_NON_INHERITED_VARIABLES_H_
