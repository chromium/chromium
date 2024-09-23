/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/dom/element_data.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct SameSizeAsElementData final
    : public GarbageCollected<SameSizeAsElementData> {
  unsigned bitfield;
  Member<void*> willbe_member;
  void* pointers[2];
};

ASSERT_SIZE(ElementData, SameSizeAsElementData);

static AdditionalBytes AdditionalBytesForShareableElementDataWithAttributeCount(
    unsigned count) {
  return AdditionalBytes(sizeof(Attribute) * count);
}

ElementData::ElementData()
    : bit_field_(IsUniqueFlag::encode(true) | ArraySize::encode(0) |
                 PresentationAttributeStyleIsDirty::encode(false) |
                 StyleAttributeIsDirty::encode(false) |
                 SvgAttributesAreDirty::encode(false)) {}

ElementData::ElementData(unsigned array_size)
    : bit_field_(IsUniqueFlag::encode(false) | ArraySize::encode(array_size) |
                 PresentationAttributeStyleIsDirty::encode(false) |
                 StyleAttributeIsDirty::encode(false) |
                 SvgAttributesAreDirty::encode(false)) {}

ElementData::ElementData(const ElementData& other, bool is_unique)
    : bit_field_(
          IsUniqueFlag::encode(is_unique) |
          ArraySize::encode(is_unique ? 0 : other.Attributes().size()) |
          PresentationAttributeStyleIsDirty::encode(
              other.bit_field_.get<PresentationAttributeStyleIsDirty>()) |
          StyleAttributeIsDirty::encode(
              other.bit_field_.get<StyleAttributeIsDirty>()) |
          SvgAttributesAreDirty::encode(
              other.bit_field_.get<SvgAttributesAreDirty>())),
      class_names_(other.class_names_),
      id_for_style_resolution_(other.id_for_style_resolution_) {
  // NOTE: The inline style is copied by the subclass copy constructor since we
  // don't know what to do with it here.
}

void ElementData::FinalizeGarbageCollectedObject() {
  if (auto* unique_element_data = DynamicTo<UniqueElementData>(this))
    unique_element_data->~UniqueElementData();
  else
    To<ShareableElementData>(this)->~ShareableElementData();
}

UniqueElementData* ElementData::MakeUniqueCopy() const {
  if (auto* unique_element_data = DynamicTo<UniqueElementData>(this))
    return MakeGarbageCollected<UniqueElementData>(*unique_element_data);
  return MakeGarbageCollected<UniqueElementData>(
      To<ShareableElementData>(*this));
}

bool ElementData::IsEquivalent(const ElementData* other) const {
  AttributeCollection attributes = Attributes();
  if (!other)
    return attributes.IsEmpty();

  AttributeCollection other_attributes = other->Attributes();
  if (attributes.size() != other_attributes.size())
    return false;

  for (const Attribute& attribute : attributes) {
    const Attribute* other_attr = other_attributes.Find(attribute.GetName());
    if (!other_attr || attribute.Value() != other_attr->Value())
      return false;
  }
  return true;
}

void ElementData::Trace(Visitor* visitor) const {
  if (bit_field_.get_concurrently<IsUniqueFlag>()) {
    static_cast<const UniqueElementData*>(this)->TraceAfterDispatch(visitor);
  } else {
    static_cast<const ShareableElementData*>(this)->TraceAfterDispatch(visitor);
  }
}

void ElementData::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(inline_style_);
  visitor->Trace(class_names_);
}

ShareableElementData::ShareableElementData(
    const Vector<Attribute, kAttributePrealloc>& attributes)
    : ElementData(attributes.size()) {
  for (unsigned i = 0; i < bit_field_.get<ArraySize>(); ++i)
    new (&attribute_array_[i]) Attribute(attributes[i]);
}

ShareableElementData::~ShareableElementData() {
  for (unsigned i = 0; i < bit_field_.get<ArraySize>(); ++i)
    attribute_array_[i].~Attribute();
}

ShareableElementData::ShareableElementData(const UniqueElementData& other)
    : ElementData(other, false) {
  DCHECK(!other.presentation_attribute_style_);

  if (other.inline_style_) {
    inline_style_ = other.inline_style_->ImmutableCopyIfNeeded();
  }

  for (unsigned i = 0; i < bit_field_.get<ArraySize>(); ++i)
    new (&attribute_array_[i]) Attribute(other.attribute_vector_.at(i));
}

ShareableElementData* ShareableElementData::CreateWithAttributes(
    const Vector<Attribute, kAttributePrealloc>& attributes) {
  return MakeGarbageCollected<ShareableElementData>(
      AdditionalBytesForShareableElementDataWithAttributeCount(
          attributes.size()),
      attributes);
}

UniqueElementData::UniqueElementData() = default;

UniqueElementData::UniqueElementData(const UniqueElementData& other)
    : ElementData(other, true),
      presentation_attribute_style_(other.presentation_attribute_style_),
      attribute_vector_(other.attribute_vector_) {
  inline_style_ =
      other.inline_style_ ? other.inline_style_->MutableCopy() : nullptr;
}

UniqueElementData::UniqueElementData(const ShareableElementData& other)
    : ElementData(other, true) {
  // An ShareableElementData should never have a mutable inline
  // CSSPropertyValueSet attached.
  DCHECK(!other.inline_style_ || !other.inline_style_->IsMutable());
  inline_style_ = other.inline_style_;

  unsigned length = other.Attributes().size();
  attribute_vector_.reserve(length);
  for (unsigned i = 0; i < length; ++i)
    attribute_vector_.UncheckedAppend(other.attribute_array_[i]);
}

ShareableElementData* UniqueElementData::MakeShareableCopy() const {
  return MakeGarbageCollected<ShareableElementData>(
      AdditionalBytesForShareableElementDataWithAttributeCount(
          attribute_vector_.size()),
      *this);
}

void UniqueElementData::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(presentation_attribute_style_);
  ElementData::TraceAfterDispatch(visitor);
}

}  // namespace blink
