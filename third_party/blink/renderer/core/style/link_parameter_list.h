// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_LINK_PARAMETER_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_LINK_PARAMETER_LIST_H_

#include <utility>

#include "base/check.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// Immutable list that preserves parameter order and duplicates.
class CORE_EXPORT LinkParameterList final
    : public GarbageCollected<LinkParameterList> {
 public:
  // Link parameter values are untyped token sequences. Context-dependent
  // tokens are interpreted where env() is used in the linked resource, so the
  // embedding document's CSSParserContext is intentionally not retained.
  class Parameter {
    DISALLOW_NEW();

   public:
    Parameter(const AtomicString& name, CSSVariableData* value)
        : name_(name), value_(value) {
      CHECK(value_);
    }

    const AtomicString& Name() const { return name_; }
    CSSVariableData* Value() const { return value_.Get(); }

    bool operator==(const Parameter& other) const {
      return name_ == other.name_ &&
             base::ValuesEquivalent(value_, other.value_);
    }

    void Trace(Visitor* visitor) const { visitor->Trace(value_); }

   private:
    AtomicString name_;
    Member<CSSVariableData> value_;
  };

  using ParameterVector = HeapVector<Parameter>;

  explicit LinkParameterList(ParameterVector parameters)
      : parameters_(std::move(parameters)) {}

  LinkParameterList(const LinkParameterList&) = delete;
  LinkParameterList& operator=(const LinkParameterList&) = delete;
  LinkParameterList(LinkParameterList&&) = delete;
  LinkParameterList& operator=(LinkParameterList&&) = delete;

  const ParameterVector& Parameters() const { return parameters_; }
  bool IsEmpty() const { return parameters_.empty(); }

  bool operator==(const LinkParameterList& other) const {
    return parameters_ == other.parameters_;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(parameters_); }

 private:
  const ParameterVector parameters_;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::LinkParameterList::Parameter)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_LINK_PARAMETER_LIST_H_
