// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/property_registry.h"

namespace blink {

void PropertyRegistry::RegisterProperty(const AtomicString& name,
                                        PropertyRegistration& registration) {
  DCHECK(!Registration(name));
  registrations_.Set(name, &registration);
}

const PropertyRegistration* PropertyRegistry::Registration(
    const AtomicString& name) const {
  return registrations_.at(name);
}

PropertyRegistry::RegistrationMap::const_iterator PropertyRegistry::begin()
    const {
  return registrations_.begin();
}

PropertyRegistry::RegistrationMap::const_iterator PropertyRegistry::end()
    const {
  return registrations_.end();
}

void PropertyRegistry::MarkReferenced(const AtomicString& property_name) const {
  const PropertyRegistration* registration = Registration(property_name);
  if (registration) {
    registration->referenced_ = true;
  }
}

bool PropertyRegistry::WasReferenced(const AtomicString& property_name) const {
  const PropertyRegistration* registration = Registration(property_name);
  if (!registration) {
    return false;
  }
  return registration->referenced_;
}

}  // namespace blink
