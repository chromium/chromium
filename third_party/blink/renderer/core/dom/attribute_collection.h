/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ATTRIBUTE_COLLECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ATTRIBUTE_COLLECTION_H_

#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

template <typename Container, typename ContainerMemberType = Container>
class AttributeCollectionGeneric {
  STACK_ALLOCATED();

 public:
  using ValueType = typename Container::ValueType;
  using iterator = ValueType*;

  AttributeCollectionGeneric(Container& attributes) : attributes_(attributes) {}

  ValueType& operator[](unsigned index) const { return at(index); }
  ValueType& at(unsigned index) const {
    CHECK_LT(index, size());
    return begin()[index];
  }

  iterator begin() const { return attributes_.data(); }
  iterator end() const { return begin() + size(); }

  unsigned size() const { return attributes_.size(); }
  bool IsEmpty() const { return !size(); }

  // Find() returns nullptr if the specified name is not found.
  iterator Find(const QualifiedName&) const;
  iterator Find(const AtomicString& name) const;
  wtf_size_t FindIndex(const QualifiedName&) const;
  wtf_size_t FindIndex(const AtomicString& name) const;

 protected:
  wtf_size_t FindSlowCase(const AtomicString& name) const;

  ContainerMemberType attributes_;
};

class AttributeArray {
  DISALLOW_NEW();

 public:
  using ValueType = const Attribute;

  AttributeArray(const Attribute* array, unsigned size)
      : array_(array), size_(size) {}

  const Attribute* data() const { return array_; }
  unsigned size() const { return size_; }

 private:
  const Attribute* array_;
  unsigned size_;
};

class AttributeCollection
    : public AttributeCollectionGeneric<const AttributeArray> {
 public:
  AttributeCollection()
      : AttributeCollectionGeneric<const AttributeArray>(
            AttributeArray(nullptr, 0)) {}

  AttributeCollection(const Attribute* array, unsigned size)
      : AttributeCollectionGeneric<const AttributeArray>(
            AttributeArray(array, size)) {}
};

using AttributeVector = Vector<Attribute, 4>;
class MutableAttributeCollection
    : public AttributeCollectionGeneric<AttributeVector, AttributeVector&> {
 public:
  explicit MutableAttributeCollection(AttributeVector& attributes)
      : AttributeCollectionGeneric<AttributeVector, AttributeVector&>(
            attributes) {}

  // These functions do no error/duplicate checking.
  void Append(const QualifiedName&, const AtomicString& value);
  void Remove(unsigned index);
};

inline void MutableAttributeCollection::Append(const QualifiedName& name,
                                               const AtomicString& value) {
  attributes_.push_back(Attribute(name, value));
}

inline void MutableAttributeCollection::Remove(unsigned index) {
  attributes_.EraseAt(index);
}

template <typename Container, typename ContainerMemberType>
inline typename AttributeCollectionGeneric<Container,
                                           ContainerMemberType>::iterator
AttributeCollectionGeneric<Container, ContainerMemberType>::Find(
    const AtomicString& name) const {
  wtf_size_t index = FindIndex(name);
  return index != kNotFound ? &at(index) : nullptr;
}

template <typename Container, typename ContainerMemberType>
inline wtf_size_t
AttributeCollectionGeneric<Container, ContainerMemberType>::FindIndex(
    const QualifiedName& name) const {
  iterator end = this->end();
  wtf_size_t index = 0;
  for (iterator it = begin(); it != end; ++it, ++index) {
    if (it->GetName().Matches(name))
      return index;
  }
  return kNotFound;
}

template <typename Container, typename ContainerMemberType>
inline wtf_size_t
AttributeCollectionGeneric<Container, ContainerMemberType>::FindIndex(
    const AtomicString& name) const {
  bool do_slow_check = false;

  // Optimize for the case where the attribute exists and its name exactly
  // matches.
  iterator end = this->end();
  wtf_size_t index = 0;
  for (iterator it = begin(); it != end; ++it, ++index) {
    // FIXME: Why check the prefix? Namespaces should be all that matter.
    // Most attributes (all of HTML and CSS) have no namespace.
    if (!it->GetName().HasPrefix()) {
      if (name == it->LocalName())
        return index;
    } else {
      do_slow_check = true;
    }
  }

  if (do_slow_check)
    return FindSlowCase(name);
  return kNotFound;
}

template <typename Container, typename ContainerMemberType>
inline typename AttributeCollectionGeneric<Container,
                                           ContainerMemberType>::iterator
AttributeCollectionGeneric<Container, ContainerMemberType>::Find(
    const QualifiedName& name) const {
  iterator end = this->end();
  for (iterator it = begin(); it != end; ++it) {
    if (it->GetName().Matches(name))
      return it;
  }
  return nullptr;
}

template <typename Container, typename ContainerMemberType>
wtf_size_t
AttributeCollectionGeneric<Container, ContainerMemberType>::FindSlowCase(
    const AtomicString& name) const {
  // Continue to checking case-insensitively and/or full namespaced names if
  // necessary:
  iterator end = this->end();
  wtf_size_t index = 0;
  for (iterator it = begin(); it != end; ++it, ++index) {
    if (!it->GetName().HasPrefix()) {
      // Skip attributes with no prefixes because they must be checked in
      // FindIndex(const AtomicString&).
      DCHECK_NE(name, it->LocalName());
    } else {
      // FIXME: Would be faster to do this comparison without calling ToString,
      // which generates a temporary string by concatenation. But this branch is
      // only reached if the attribute name has a prefix, which is rare in HTML.
      if (name == it->GetName().ToString())
        return index;
    }
  }
  return kNotFound;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ATTRIBUTE_COLLECTION_H_
