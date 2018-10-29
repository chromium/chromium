// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTY_REGISTRATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTY_REGISTRATION_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolation_types_map.h"
#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_variable_data.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class PropertyDescriptor;

using CSSInterpolationTypes = Vector<std::unique_ptr<CSSInterpolationType>>;

class CORE_EXPORT PropertyRegistration
    : public GarbageCollectedFinalized<PropertyRegistration> {
 public:
  static void registerProperty(ExecutionContext*,
                               const PropertyDescriptor&,
                               ExceptionState&);

  static const PropertyRegistration* From(const ExecutionContext*,
                                          const AtomicString& property_name);

  const CSSSyntaxDescriptor& Syntax() const { return syntax_; }
  bool Inherits() const { return inherits_; }
  const CSSValue* Initial() const { return initial_; }
  CSSVariableData* InitialVariableData() const {
    return initial_variable_data_.get();
  }
  const InterpolationTypes& GetInterpolationTypes() const {
    return interpolation_types_;
  }

  void Trace(blink::Visitor* visitor) { visitor->Trace(initial_); }

 private:
  friend class ::blink::PropertyRegistry;

  PropertyRegistration(const AtomicString& name,
                       const CSSSyntaxDescriptor&,
                       bool inherits,
                       const CSSValue* initial,
                       scoped_refptr<CSSVariableData> initial_variable_data);

  const CSSSyntaxDescriptor syntax_;
  const bool inherits_;
  const Member<const CSSValue> initial_;
  const scoped_refptr<CSSVariableData> initial_variable_data_;
  const InterpolationTypes interpolation_types_;
  mutable bool referenced_;

  FRIEND_TEST_ALL_PREFIXES(CSSVariableResolverTest,
                           NeedsResolutionClearedByResolver);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTY_REGISTRATION_H_
