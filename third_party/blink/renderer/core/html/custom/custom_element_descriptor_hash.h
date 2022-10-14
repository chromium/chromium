// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DESCRIPTOR_HASH_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DESCRIPTOR_HASH_H_

#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

struct CustomElementDescriptorHash {
  STATIC_ONLY(CustomElementDescriptorHash);
  static unsigned GetHash(const CustomElementDescriptor& descriptor) {
    return WTF::HashInts(AtomicStringHash::GetHash(descriptor.GetName()),
                         AtomicStringHash::GetHash(descriptor.LocalName()));
  }

  static bool Equal(const CustomElementDescriptor& a,
                    const CustomElementDescriptor& b) {
    return a == b;
  }

  static const bool safe_to_compare_to_empty_or_deleted = true;
};

}  // namespace blink

namespace WTF {

template <>
struct HashTraits<blink::CustomElementDescriptor>
    : SimpleClassHashTraits<blink::CustomElementDescriptor> {
  STATIC_ONLY(HashTraits);
  static const bool kEmptyValueIsZero =
      HashTraits<AtomicString>::kEmptyValueIsZero;

  static bool IsDeletedValue(const blink::CustomElementDescriptor& value) {
    return HashTraits<AtomicString>::IsDeletedValue(value.name_);
  }

  static void ConstructDeletedValue(blink::CustomElementDescriptor& slot,
                                    bool zero_value) {
    HashTraits<AtomicString>::ConstructDeletedValue(slot.name_, zero_value);
  }
};

template <>
struct DefaultHash<blink::CustomElementDescriptor>
    : blink::CustomElementDescriptorHash {};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_DESCRIPTOR_HASH_H_
