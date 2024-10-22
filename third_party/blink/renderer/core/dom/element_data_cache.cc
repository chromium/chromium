/*
 * Copyright (C) 2012, 2013 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/dom/element_data_cache.h"

#include "third_party/blink/renderer/core/dom/element_data.h"

namespace blink {

inline unsigned AttributeHash(
    const Vector<Attribute, kAttributePrealloc>& attributes) {
  return StringHasher::HashMemory(attributes.data(),
                                  attributes.size() * sizeof(Attribute));
}

inline bool HasSameAttributes(
    const Vector<Attribute, kAttributePrealloc>& attributes,
    ShareableElementData& element_data) {
  return std::equal(
      attributes.begin(), attributes.end(), element_data.attribute_array_,
      element_data.attribute_array_ + element_data.Attributes().size());
}

ShareableElementData*
ElementDataCache::CachedShareableElementDataWithAttributes(
    const Vector<Attribute, kAttributePrealloc>& attributes) {
  DCHECK(!attributes.empty());

  ShareableElementDataCache::ValueType* it =
      shareable_element_data_cache_.insert(AttributeHash(attributes), nullptr)
          .stored_value;

  // FIXME: This prevents sharing when there's a hash collision.
  if (it->value && !HasSameAttributes(attributes, *it->value))
    return ShareableElementData::CreateWithAttributes(attributes);

  if (!it->value)
    it->value = ShareableElementData::CreateWithAttributes(attributes);

  return it->value.Get();
}

ElementDataCache::ElementDataCache() = default;

void ElementDataCache::Trace(Visitor* visitor) const {
  visitor->Trace(shareable_element_data_cache_);
}

}  // namespace blink
