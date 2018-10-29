// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTY_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTY_REGISTRY_H_

#include "third_party/blink/renderer/core/css/property_registration.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class CORE_EXPORT PropertyRegistry : public GarbageCollected<PropertyRegistry> {
 public:
  using RegistrationMap =
      HeapHashMap<AtomicString, Member<PropertyRegistration>>;

  static PropertyRegistry* Create() { return new PropertyRegistry(); }

  void RegisterProperty(const AtomicString&, PropertyRegistration&);
  const PropertyRegistration* Registration(const AtomicString&) const;
  size_t RegistrationCount() const { return registrations_.size(); }

  RegistrationMap::const_iterator begin() const;
  RegistrationMap::const_iterator end() const;

  void Trace(blink::Visitor* visitor) { visitor->Trace(registrations_); }

  // Parse the incoming value and return the parsed result, if:
  //  1. A registration with the specified name exists, and
  //  2. The incoming value is a CSSCustomPropertyDeclaration, has no
  //     unresolved var-references and matches the registered syntax.
  // Otherwise the incoming value is returned.
  static const CSSValue* ParseIfRegistered(const Document& document,
                                           const AtomicString& property_name,
                                           const CSSValue*);

  void MarkReferenced(const AtomicString&) const;
  bool WasReferenced(const AtomicString&) const;

 private:
  RegistrationMap registrations_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTY_REGISTRY_H_
