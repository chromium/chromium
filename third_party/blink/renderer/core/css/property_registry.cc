// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/property_registry.h"

namespace blink {

void PropertyRegistry::RegisterProperty(const AtomicString& name,
                                        PropertyRegistration& registration) {
  DCHECK(!IsInRegisteredPropertySet(name));
  registered_properties_.Set(name, &registration);
  registered_viewport_unit_flags_ |= registration.GetViewportUnitFlags();
  version_++;
}

void PropertyRegistry::DeclareProperty(const AtomicString& name,
                                       PropertyRegistration& registration) {
  declared_properties_.Set(name, &registration);
  declared_viewport_unit_flags_ |= registration.GetViewportUnitFlags();
  version_++;
}

void PropertyRegistry::RemoveDeclaredProperties() {
  if (declared_properties_.empty()) {
    return;
  }
  declared_properties_.clear();
  declared_viewport_unit_flags_ = 0;
  version_++;
}

const PropertyRegistration* PropertyRegistry::Registration(
    const AtomicString& name) const {
  // If a property is registered with both CSS.registerProperty and @property,
  // the registration from CSS.registerProperty must win.
  //
  // https://drafts.css-houdini.org/css-properties-values-api-1/#determining-registration
  auto it = registered_properties_.find(name);
  if (it != registered_properties_.end()) {
    return it->value.Get();
  }
  it = declared_properties_.find(name);
  return it != declared_properties_.end() ? it->value : nullptr;
}

bool PropertyRegistry::IsEmpty() const {
  return registered_properties_.empty() && declared_properties_.empty();
}

bool PropertyRegistry::IsInRegisteredPropertySet(
    const AtomicString& name) const {
  return registered_properties_.Contains(name);
}

PropertyRegistry::Iterator::Iterator(
    const RegistrationMap& registered_properties,
    const RegistrationMap& declared_properties,
    MapIterator registered_iterator,
    MapIterator declared_iterator)
    : registered_iterator_(registered_iterator),
      declared_iterator_(declared_iterator),
      registered_properties_(registered_properties),
      declared_properties_(declared_properties) {}

// The iterator works by first yielding the CSS.registerProperty-registrations
// unconditionally (since nothing can override them), and then yield the
// @property-registrations that aren't masked by conflicting
// CSS.registerProperty-registrations.
void PropertyRegistry::Iterator::operator++() {
  if (registered_iterator_ != registered_properties_.end()) {
    ++registered_iterator_;
  } else {
    ++declared_iterator_;
  }

  if (registered_iterator_ == registered_properties_.end()) {
    while (CurrentDeclaredIteratorIsMasked()) {
      ++declared_iterator_;
    }
  }
}

PropertyRegistry::RegistrationMap::ValueType
PropertyRegistry::Iterator::operator*() const {
  if (registered_iterator_ != registered_properties_.end()) {
    return *registered_iterator_;
  }
  return *declared_iterator_;
}

bool PropertyRegistry::Iterator::operator==(const Iterator& o) const {
  return registered_iterator_ == o.registered_iterator_ &&
         declared_iterator_ == o.declared_iterator_;
}

bool PropertyRegistry::Iterator::CurrentDeclaredIteratorIsMasked() {
  return (declared_iterator_ != declared_properties_.end()) &&
         registered_properties_.Contains(declared_iterator_->key);
}

PropertyRegistry::Iterator PropertyRegistry::begin() const {
  return Iterator(registered_properties_, declared_properties_,
                  registered_properties_.begin(), declared_properties_.begin());
}

PropertyRegistry::Iterator PropertyRegistry::end() const {
  return Iterator(registered_properties_, declared_properties_,
                  registered_properties_.end(), declared_properties_.end());
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
